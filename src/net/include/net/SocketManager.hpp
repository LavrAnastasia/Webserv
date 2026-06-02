#pragma once

#include <vector>

#include "ServerSocket.hpp"
#include "config/ServerConfig.hpp"

class SocketManager {
private:
    std::vector<ServerSocket*> servers_;

public:
    SocketManager() = default;
    ~SocketManager();

    void createServers(const std::vector<ServerConfig>& configs);
    const std::vector<ServerSocket*>& getServers() const;
};
