#include "SocketManager.hpp"

SocketManager::~SocketManager() {
    for (ServerSocket* server : servers_) {
        delete server;
    }
    servers_.clear();
}

void SocketManager::createServers(const std::vector<ServerConfig>& configs) {
}

const std::vector<ServerSocket*>& SocketManager::getServers() const {
}
/*
    Range-based loop syntax:
    for (ServerSocket* server : servers_) {
    delete server;
}
    1. ServerSocket* server <- current item (temporary variable)
    2. : <- reads as "in"
    3. servers_ <- location of items being operated on

*/
