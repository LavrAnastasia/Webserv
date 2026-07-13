#pragma once

#include "config/ServerConfig.hpp"
#include "net/ServerSocket.hpp"

#include <memory> // needed for std::unique_ptr
#include <unordered_map>
#include <vector>

class SocketManager {
private:
    /*
    Changed raw pointers to smart (unique) pointers. Unique pointers owned by
    SocketManager are deleted automatically if SocketManager is destroyed.
    */
    std::vector<std::unique_ptr<ServerSocket>> servers_;
    // listenFD -> ServerConfig
    std::unordered_map<int, const ServerConfig*> fdToConfig_;

public:
    SocketManager() = default;

    void createServers(const std::vector<ServerConfig>& configs);

    // used ultimately by EventLoop to set up poll()
    std::vector<int> getListeningFds() const;

    // used by TcpServer to get rules for each socket
    const ServerConfig* getConfigForFd(int fd) const;

    // used by TcpServer to accept connection
    ServerSocket* getServerByFd(int fd);
};
