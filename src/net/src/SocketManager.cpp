#include "net/SocketManager.hpp"
#include "config/ServerConfig.hpp"
#include "net/ServerSocket.hpp"

//destructor impementation not needed with smart pointers

void SocketManager::createServers(const std::vector<ServerConfig>& configs) {
    //1. loop through server configs
    for (const ServerConfig& config : configs) {
        //2. loop through listen configs in each server config
        for (const ListenConfig& listenBlock : config.listen) {
            //3. create smart pointer
            auto newSocket = std::make_unique<ServerSocket>(listenBlock.host, listenBlock.port);
            //4. configure socket
            newSocket->setNonBlocking();
            //5. unique pointer cannot be copied and must be moved into vector
            servers_.push_back(std::move(newSocket));
        }
    }
}


std::vector<ServerSocket*> SocketManager::getServers() const {
    std::vector<ServerSocket*> rawPointers;

    for (const auto& serverPtr : servers_) {
        //get() returns raw pointer address without transferring ownership
        rawPointers.push_back(serverPtr.get());
    }
    return rawPointers;
}
