#include "net/EventLoop.hpp"
#include "net/TcpServer.hpp"
#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <unistd.h> //for close()
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
            // TODO: WEB-28 RequestHandler integration
            // TODO: WEB-17 HttpSerializer integration
            //PLACEHOLDER RESPONSE: get path from parsed request
            std::string path = result.request->path;
            //PLACEHOLDER RESPONSE: create body
            std::string body = "<html><body><h1>:)</h1><p>Requested: " + path + "</p></body></html>";
            //PLACEHOLDER RESPONSE: create full response
            std::string response = "HTTP/1.1 200 OK\r\n"
                                   "Content-Length: " +
                std::to_string(body.length()) +
                "\r\n"
                "Content-Type: text/html\r\n"
                "\r\n" +
                body;
            connection->appendResponse(response);
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
        else if (result.status == ParseStatus::BadRequest) {
            // TODO: WEB-29 ErrorResponseFactory integration
            connection->appendResponse("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
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
                poller_.modifySocket(clientFd, POLLIN);
            }
        }
    }
}

void EventLoop::initialize() {
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
