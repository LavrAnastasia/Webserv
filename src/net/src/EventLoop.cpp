#include "net/EventLoop.hpp"
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
        ParseResult result = connection->receiveRequest();

        // Parsing complete -> build response from HttpRequest
        if (result.status == ParseStatus::Complete) {
            /*
                TODO: Replace raw strings "connection" & "close" with constants
                from Http::Headers once available
            */
            std::optional<std::string> connHeader = result.request->headers.get("connection");

            if (connHeader.has_value()) {
                if (HttpHeaders::equals(connHeader.value(), "close")) {
                    connection->setShouldClose(true);
                } else {
                    connection->setShouldClose(false);
                }
            } else {
                // default to keep-alive
                connection->setShouldClose(false);
            }

            HttpResponse response = RequestHandler::handle(*result.request, connection->getServerConfig());
            connection->appendResponse(HttpSerializer::serialize(response));
            poller_.modifySocket(clientFd, POLLOUT);
        }

        // Client disconnected -> clean up immediately
        else if (result.status == ParseStatus::ConnectionClosed) {
            poller_.removeSocket(clientFd);
            connectionRegistry_.removeConnection(clientFd);
            return;
        }

        /*
        fail case: unable to parse client request -> build error response and
        close connection after sending!
        */
        // TODO: WEB-26 Every failure collapses into BadRequest, so 501/505/413/431/414 are lost
        else if (result.status == ParseStatus::BadRequest) {
            connection->appendResponse(HttpSerializer::serialize(RequestHandler::reject(HttpStatus::BadRequest)));
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
    isRunning_ = true;

    while (isRunning_) {
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
    //pruneConnections() removes timed out connections from registry and returns list of their fds
    std::vector<int> deadFds =
        connectionRegistry_.pruneConnections(clientTimeoutSeconds_, std::chrono::steady_clock::now());
    for (int fd : deadFds) {
        //poller removes them from its own internal list
        poller_.removeSocket(fd);
    }
}

void EventLoop::stop() {
    isRunning_ = false;
}
