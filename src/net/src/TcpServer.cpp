#include "net/TcpServer.hpp"
#include "ServerSocket.hpp"

/*
    Initialization: Parsed config blocks are passed to SocketManager, which creates,
    binds and sets sockets to listen mode.

    -Implementation already handled in SocketManager class
*/
void TcpServer::setupServers(const std::vector<ServerConfig>& configs) {
    socketManager_.createServers(configs);
}

/*
    Called once by EventLoop at startup, to get list of listening FDs to
    register with Poller

    -function return reference can be used directly in the range-based loop
*/
std::vector<int> TcpServer::getListeningFds() const {
    std::vector<int> fds;
    for (const ServerSocket* server : socketManager_.getServers()) {
        fds.push_back(server->getFd());
    }
    return fds;
}


/*
    Called by EventLoop when Poller triggers a POLLIN on a listening port.
    Finds the corresponding ServerSocket object and tells it to accept the new client.
    Obtains client info from the ServerSocket and returns it as a ClientInfo object.
    ClientInfo ip and port variables are passed by reference and populated by
    acceptConnection()
*/
TcpServer::ClientInfo TcpServer::acceptClient(int listenFd) {
    ClientInfo info;
    info.fd = -1;

    for (ServerSocket* server : socketManager_.getServers()) {
        if (server->getFd() == listenFd) {
            info.fd = server->acceptConnection(info.ip, info.port);
            return info;
        }
    }
    return info;
}
