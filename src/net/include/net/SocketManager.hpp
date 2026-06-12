#pragma once

#include <vector>

#include "config/ServerConfig.hpp"

class ServerSocket;

class SocketManager {
private:
    std::vector<ServerSocket*> servers_;

public:
    SocketManager() = default;
    ~SocketManager();

    void createServers(const std::vector<ServerConfig>& configs);
    const std::vector<ServerSocket*>& getServers() const;
};
