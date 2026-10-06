#include "net/SocketManager.hpp"
#include "config/ServerConfig.hpp"
#include "log/Log.hpp"
#include "net/ServerSocket.hpp"

#include <stdexcept>
#include <string>
#include <system_error>

//destructor impementation not needed with smart pointers

void SocketManager::createServers(const std::vector<ServerConfig>& configs) {
    //1. loop through server configs
    for (const ServerConfig& config : configs) {
        //2. loop through listen configs in each server config
        for (const ListenConfig& listenBlock : config.listen) {
            try {
                //3. create smart pointer
                auto newSocket = std::make_unique<ServerSocket>(listenBlock.host, listenBlock.port);
                //4. configure socket
                newSocket->setNonBlocking();
                newSocket->setCloseOnExec();
                //5. Map fd to config block
                int fd = newSocket->getFd();
                fdToConfig_[fd] = &config;
                //6. unique pointer cannot be copied and must be moved into vector
                servers_.push_back(std::move(newSocket));
                Log::info("listening on " + listenBlock.host + ":" + std::to_string(listenBlock.port));
            } catch (const std::system_error& e) {
                // error is logged, but valid ports start up normally
                Log::warn(
                    "failed to start listener on " + listenBlock.host + ":" + std::to_string(listenBlock.port) + ": " +
                    e.what()
                );
            }
        }
    }
    if (servers_.empty()) {
        throw std::runtime_error("no listening sockets could be opened");
    }
}

/*
    Returns a list of fds for the EventLoop to register with Poller
*/
std::vector<int> SocketManager::getListeningFds() const {
    std::vector<int> listeningFds;
    //loop through key-value pairs of fdToConfig_
    for (auto& pair : fdToConfig_) {
        //add fds to return vector
        listeningFds.push_back(pair.first);
    }
    return listeningFds;
}

/*
    Efficient lookup for the specific configuration associated with
    a listening port, called when Poller detects activity
*/
const ServerConfig* SocketManager::getConfigForFd(int fd) const {
    auto it = fdToConfig_.find(fd);
    if (it != fdToConfig_.end()) {
        return it->second;
    }
    return nullptr;
}

/*
    finds correct ServerSocket object for accept() call
    get() returns raw pointer to unique_ptr object, which grants
    access and ability to call functions on it, but not ownership
    release() would transfer ownership and responsibility for deletion
*/
ServerSocket* SocketManager::getServerByFd(int fd) {
    for (auto& server : servers_) {
        if (server->getFd() == fd) {
            return server.get();
        }
    }
    return nullptr;
}
