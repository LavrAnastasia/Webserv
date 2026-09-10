#include "net/EventLoop.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpSerializer.hpp"
#include "http/RequestHandler.hpp"
#include "net/TcpServer.hpp"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <unistd.h>
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
                connectionRegistry_.removeConnection(clientInfo->fd);
                poller_.removeSocket(clientInfo->fd);
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
        poller_.removeSocket(clientFd);
        connectionRegistry_.removeConnection(clientFd);
        return;
    }
    //reading phase (OS kernel receive buffer has data)
    if (events & POLLIN) {
        //feed bytes from OS kernel's socket buffer into parser and receive status
        const std::optional<ParseResult> received = connection->receiveRequest();

        // Client disconnected -> clean up immediately
        if (!received) {
            poller_.removeSocket(clientFd);
            connectionRegistry_.removeConnection(clientFd);
            return;
        }

        const ParseResult& result = *received;

        // Parsing complete -> build response from HttpRequest
        if (const Complete* complete = std::get_if<Complete>(&result)) {
            HttpResponse response = RequestHandler::handle(complete->request, connection->getServerConfig());

            connection->appendResponse(HttpSerializer::serialize(response));
            connection->setShouldClose(!complete->request.isPersistent());
            poller_.modifySocket(clientFd, POLLOUT);
        }

        /*
        fail case: unable to parse client request -> build error response and
        close connection after sending!
        */
        else if (const Failed* failed = std::get_if<Failed>(&result)) {
            connection->appendResponse(HttpSerializer::serialize(RequestHandler::reject(failed->status)));
            connection->setShouldClose(true);
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
            poller_.removeSocket(clientFd);
            connectionRegistry_.removeConnection(clientFd);
            return;
        }

        if (connection->isSendComplete()) {
            if (connection->shouldClose()) {
                poller_.removeSocket(clientFd);
                connectionRegistry_.removeConnection(clientFd);
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

        // connection already flagged to close, but timed out before closing
        // close immediately, do not queue another 408
        if (connection->shouldClose()) {
            std::cout << "webserv: info: fd " << fd << " timed out while waiting to close. Closing immediately."
                      << std::endl;
            poller_.removeSocket(fd);
            connectionRegistry_.removeConnection(fd);
            continue;
        }

        // response still pending after inactivity timeout
        // close immediately, do not queue 408
        if (!connection->isSendComplete()) {
            std::cout << "webserv: info: fd " << fd << " timed out while sending response. Closing immediately."
                      << std::endl;
            poller_.removeSocket(fd);
            connectionRegistry_.removeConnection(fd);
            continue;
        }

        try {
            // no response pending: client timed out while sending request
            HttpResponse res = RequestHandler::reject(HttpStatus::RequestTimeout);
            connection->appendResponse(HttpSerializer::serialize(res));
            connection->setShouldClose(true);
            poller_.modifySocket(fd, POLLOUT);
            std::cout << "webserv: info: fd " << fd << " timed out. Sending 408." << std::endl;
        } catch (const std::exception&) {
            // if preparing or queueing response fails, remove connection immediately
            poller_.removeSocket(fd);
            connectionRegistry_.removeConnection(fd);
        }
    }
}

void EventLoop::stop() {
    stopRequested_ = 1;
}
