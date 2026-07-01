#include "net/EventLoop.hpp"
#include "net/TcpServer.hpp"
#include <algorithm>
#include <vector>

void EventLoop::handleNewConnection(int listenFd) {
    //accept client and get info
    TcpServer::ClientInfo clientInfo = tcpServer_.acceptClient(listenFd);

    //check for dropped connection
    if (clientInfo.fd != -1) {
        //get config block for this port
        const ServerConfig* config = tcpServer_.getServerConfigByPort(clientInfo.serverPort);
        //add new connection to registry, including config block
        connectionRegistry_.addConnection(clientInfo.fd, clientInfo.ip, config);
        //tell poller to track it (watch for incoming http request)
        poller_.addSocket(clientInfo.fd);
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
    //receive phase (OS read bucket has data)
    if (events & POLLIN) {
        if (!connection->receiveRequest()) {
            //client disconnected
            poller_.removeSocket(clientFd);
            connectionRegistry_.removeConnection(clientFd);
            return;
        }
        if (connection->hasCompleteHeaders()) {
            //if we finished receiving the entire HTTP request:
            //PLACEHOLDER RESPONSE! This will integrate with the HTTP parser later!
            connection->appendResponse(
                "HTTP/1.1 200 OK\r\nContent-Length: 37\r\n\r\n<html><body><h1>:)</h1></body></html>"
            );
            poller_.modifySocket(clientFd, POLLOUT);
        }
    }
    //send phase (OS write bucket has space)
    if (events & POLLOUT) {
        if (!connection->sendResponse()) {
            //client disconnected
            poller_.removeSocket(clientFd);
            connectionRegistry_.removeConnection(clientFd);
            return;
        }
        //sendResponse() sets state to READING_HEADERS when send is complete (buffer is empty)
        if (connection->getState() == Connection::State::READING_HEADERS) {
            //tells poller send phase is done, now watch for new requests
            poller_.modifySocket(clientFd, POLLIN);
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
    std::vector<int> deadFds = connectionRegistry_.pruneConnections(clientTimeoutSeconds_);
    for (int fd : deadFds) {
        //poller removes them from its own internal list
        poller_.removeSocket(fd);
    }
}

void EventLoop::stop() {
    isRunning_ = false;
}
