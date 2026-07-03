#include "net/TcpServer.hpp"
#include "net/ServerSocket.hpp"

TcpServer::TcpServer(const Configuration& config) : config_(config) {
    socketManager_.createServers(config_.servers);
}

/*
    Called once by EventLoop at startup, to get list of listening FDs to
    register with Poller
*/
std::vector<int> TcpServer::getListeningFds() const {
    return socketManager_.getListeningFds();
}

/*
    Called by EventLoop to retrieve the correct configuration for a given fd
*/
const ServerConfig* TcpServer::getConfigForFd(int listenFd) const {
    return socketManager_.getConfigForFd(listenFd);
}

/*
    Called by EventLoop when Poller triggers a POLLIN on a listening port.
    Finds the corresponding ServerSocket object and tells it to accept the new client.
    Obtains client info from the ServerSocket and returns it as a ClientInfo object.
    ClientInfo ip and port variables are passed by reference and populated by
    acceptConnection()
*/
std::optional<TcpServer::ClientInfo> TcpServer::acceptClient(int listenFd) {
    ServerSocket* server = socketManager_.getServerByFd(listenFd);
    if (!server) {
        //magic number replaced by nullopt
        return std::nullopt;
    }

    ClientInfo info;
    info.fd = server->acceptConnection(info.ip, info.port);

    if (info.fd < 0) {
        return std::nullopt;
    }

    info.serverPort = server->getPort();

    return info;
}
