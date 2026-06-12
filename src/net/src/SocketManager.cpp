#include "net/SocketManager.hpp"
#include "ServerSocket.hpp"
#include "config/ServerConfig.hpp"

SocketManager::~SocketManager() {
    for (ServerSocket* server : servers_) {
        delete server;
    }
    servers_.clear();
}

/*
    const std::vector<ServerConfig>& configs
    "A read-only reference to a vector of ServerConfig objects, named configs"
*/

void SocketManager::createServers(const std::vector<ServerConfig>& configs) {
    //1. loop through server configs
    for (const ServerConfig& config : configs) {
        //2. loop through listen configs in each server config
        for (const ListenConfig& listenBlock : config.listen) {
            //3. create socket, set it to non-blocking and save it in servers_
            ServerSocket* newSocket = new ServerSocket(listenBlock.host, listenBlock.port);
            newSocket->setNonBlocking();
            servers_.push_back(newSocket);
        }
    }
}

/*
    when function signature specifies return by reference, '&' not needed in return statement
*/

const std::vector<ServerSocket*>& SocketManager::getServers() const {
    return servers_;
}

/*
    Range-based loop syntax:
    for (ServerSocket* server : servers_) {
    delete server;
}
    1. ServerSocket* server <- current item (temporary variable)
    2. : <- reads as "in"
    3. servers_ <- location of items being operated on

    Write a loop to go through the configs vector (by const & so you don't copy the
    massive structs).

    Inside that loop, write another loop to go through the config.listen vector
    (also by const &).

    Inside the inner loop, you will extract the port,
    build the socket, make it non-blocking, and save it to servers_.


    struct ServerConfig {
    std::vector<ListenConfig> listen;

    std::filesystem::path root;
    std::string index;
    std::size_t clientMaxBodySize;

    std::unordered_map<int, std::filesystem::path> errorPages;

    std::vector<LocationConfig> locations;
};


    struct ListenConfig {
    std::string host;
    std::uint16_t port;
};

*/
