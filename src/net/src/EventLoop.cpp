#include "net/EventLoop.hpp"
#include "http/CgiResponseParser.hpp"
#include "http/HttpResponse.hpp"
#include "http/RequestDispatcher.hpp"
#include "net/TcpServer.hpp"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <optional>
#include <unistd.h>
#include <utility>
#include <vector>

volatile std::sig_atomic_t EventLoop::stopRequested_ = 0;

void EventLoop::setupSignals() {
    std::signal(SIGINT, EventLoop::handleSignal);
    std::signal(SIGTERM, EventLoop::handleSignal);
}

void EventLoop::handleSignal(int sig) {
    (void)sig;
    stopRequested_ = 1;
}

void EventLoop::handleNewConnection(int listenFd) {
    //auto allows nullopt return
    auto clientInfo = tcpServer_.acceptClient(listenFd);

    //check for dropped connection
    if (clientInfo) {
        //get config block for this fd
        const ServerConfig* config = tcpServer_.getConfigForFd(listenFd);
        if (config) {
            try {
                //add new connection to registry, including config block
                connectionRegistry_.addConnection(clientInfo->fd, clientInfo->ip, config);
                //tell poller to track it (watch for incoming http request)
                poller_.addSocket(clientInfo->fd);
            } catch (const std::exception& e) {
                // in case of setNonBlocking() failure, log error and remove connection + socket
                std::cerr << "NetError: Failed to initialize client " << clientInfo->ip << " - " << e.what()
                          << std::endl;
                closeConnection(clientInfo->fd);
            }
        } else {
            // if config for accepted client not found, close connection
            close(clientInfo->fd);
        }
    }
    //if connection was dropped, do nothing
}

void EventLoop::handleClientActivity(int clientFd, uint32_t events) {
    //fetch client
    Connection* connection = connectionRegistry_.getConnection(clientFd);
    if (connection == nullptr) {
        return;
    }
    //handle errors and disconnects (POLLERR and POLLHUP)
    if (events & (POLLERR | POLLHUP)) {
        closeConnection(clientFd);
        return;
    }
    //reading phase (OS kernel receive buffer has data)
    if (events & POLLIN) {
        //feed bytes from OS kernel's socket buffer into parser and receive status
        const std::optional<ParseResult> received = connection->receiveRequest();

        // Client disconnected -> clean up immediately
        if (!received) {
            closeConnection(clientFd);
            return;
        }

        const ParseResult& result = *received;

        // Parsing complete -> build response from HttpRequest
        if (const Complete* complete = std::get_if<Complete>(&result)) {
            const ServerConfig& server = connection->getServerConfig();
            HandlerResult handlerResult = RequestDispatcher::dispatch(complete->request, server);

            connection->setShouldClose(!complete->request.isPersistent());

            if (const HttpResponse* response = std::get_if<HttpResponse>(&handlerResult)) {
                connection->appendResponse(*response);
                poller_.modifySocket(clientFd, POLLOUT);
            } else {
                launchCgi(*connection, std::get<CgiRequest>(handlerResult));
            }
        }

        /*
        fail case: unable to parse client request -> build error response and
        close connection after sending!
        */
        else if (const Failed* failed = std::get_if<Failed>(&result)) {
            connection->setShouldClose(true);
            connection->appendResponse(RequestDispatcher::fail(failed->status, connection->getServerConfig()));
            poller_.modifySocket(clientFd, POLLOUT); //switch to POLLOUT to send error
        }
        /*
            case 'NeedMoreData' -> do nothing and wait for next loop - no POLLOUT switch
        */
    }

    //send phase (OS write bucket has space)
    if (events & POLLOUT) {
        if (!connection->sendResponse()) {
            //client disconnected
            closeConnection(clientFd);
            return;
        }

        if (connection->isSendComplete()) {
            if (connection->shouldClose()) {
                closeConnection(clientFd);
            } else {
                connection->resetParser();
                /*
                    TODO: HTTP pipelining support:
                    If client has sent multiple requests and parser buffer still
                    has data in it after reset, parser should be re-run immediately.
                    Requests already in buffer need to be processed before setting
                    sockete to POLLIN.
                */
                poller_.modifySocket(clientFd, POLLIN);
            }
        }
    }
}

void EventLoop::launchCgi(Connection& connection, const CgiRequest& request) {
    std::optional<CgiProcess> process = CgiProcess::launch(request);

    if (!process) {
        connection.appendResponse(RequestDispatcher::fail(HttpStatus::BadGateway, connection.getServerConfig()));
        poller_.modifySocket(connection.getFd(), POLLOUT);
        return;
    }

    if (process->inputFd() >= 0) {
        poller_.addSocket(process->inputFd());
        poller_.modifySocket(process->inputFd(), POLLOUT);
    }

    poller_.addSocket(process->outputFd());
    poller_.modifySocket(connection.getFd(), 0);
    cgiRegistry_.add(connection.getFd(), std::move(*process));
}

void EventLoop::handleCgiActivity(int clientFd, int pipeFd) {
    CgiProcess* process = cgiRegistry_.find(clientFd);
    const int input = process->inputFd();
    const int output = process->outputFd();

    if (pipeFd == input) {
        process->writeInput();
    } else {
        process->readOutput();
    }

    if (process->inputFd() != input) {
        poller_.removeSocket(input);
    }

    if (process->outputFd() != output) {
        poller_.removeSocket(output);
    }

    if (process->isAlive()) {
        return;
    }

    Connection* connection = connectionRegistry_.getConnection(clientFd);
    const std::optional<HttpResponse> response = CgiResponseParser::parse(process->output());

    if (response) {
        connection->appendResponse(*response);
    } else {
        connection->appendResponse(RequestDispatcher::fail(HttpStatus::BadGateway, connection->getServerConfig()));
    }

    cgiRegistry_.remove(clientFd);
    poller_.modifySocket(clientFd, POLLOUT);
}

void EventLoop::closeConnection(int fd) {
    if (CgiProcess* process = cgiRegistry_.find(fd)) {
        poller_.removeSocket(process->inputFd());
        poller_.removeSocket(process->outputFd());
        cgiRegistry_.remove(fd);
    }

    poller_.removeSocket(fd);
    connectionRegistry_.removeConnection(fd);
}

void EventLoop::initialize() {
    std::signal(SIGPIPE, SIG_IGN);

    listeningFds_ = tcpServer_.getListeningFds();
    for (int fd : listeningFds_) {
        poller_.addSocket(fd);
    }
}

void EventLoop::run() {
    while (stopRequested_ == 0) {
        // poller returns vector<pollfd> of active sockets (incl. *what* activity)
        auto activeSockets = poller_.waitForEvents();
        for (pollfd& event : activeSockets) {
            auto it = std::find(listeningFds_.begin(), listeningFds_.end(), event.fd);

            //if active fd is in listeningFds_, it's a new connection
            if (it != listeningFds_.end()) {
                handleNewConnection(event.fd);
            } else if (std::optional<int> client = cgiRegistry_.client(event.fd)) {
                handleCgiActivity(*client, event.fd);
            }
            //if not, it's an existing client
            else {
                handleClientActivity(event.fd, event.revents);
            }
        }
        cleanupTimedOutConnections();
    }
}

void EventLoop::cleanupTimedOutConnections() {
    std::vector<int> timedOutFds =
        connectionRegistry_.getTimedOutConnections(clientTimeoutSeconds_, std::chrono::steady_clock::now());

    for (int fd : timedOutFds) {
        Connection* connection = connectionRegistry_.getConnection(fd);

        if (!connection)
            continue;

        if (cgiRegistry_.find(fd)) {
            continue;
        }

        // connection already flagged to close, but timed out before closing
        // close immediately, do not queue another 408
        if (connection->shouldClose()) {
            std::cout << "webserv: info: fd " << fd << " timed out while waiting to close. Closing immediately."
                      << std::endl;
            closeConnection(fd);
            continue;
        }

        // response still pending after inactivity timeout
        // close immediately, do not queue 408
        if (!connection->isSendComplete()) {
            std::cout << "webserv: info: fd " << fd << " timed out while sending response. Closing immediately."
                      << std::endl;
            closeConnection(fd);
            continue;
        }

        // no response pending: client timed out while sending request
        connection->setShouldClose(true);
        connection->appendResponse(RequestDispatcher::fail(HttpStatus::RequestTimeout, connection->getServerConfig()));
        poller_.modifySocket(fd, POLLOUT);
        std::cout << "webserv: info: fd " << fd << " timed out. Sending 408." << std::endl;
    }
}

void EventLoop::stop() {
    stopRequested_ = 1;
}
