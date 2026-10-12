#include "net/ConnectionRegistry.hpp"

ConnectionRegistry::Client::Client(int fd, const ServerConfig& server, const ConnectionInfo& info)
    : connection(fd, info), session(server, info) {
}

void ConnectionRegistry::addConnection(
    int fd, const std::string& ip, std::uint16_t serverPort, const ServerConfig* config
) {
    clients_.try_emplace(fd, fd, *config, ConnectionInfo{ip, serverPort});
}

void ConnectionRegistry::removeConnection(int fd) {
    clients_.erase(fd);
}

ConnectionRegistry::Client* ConnectionRegistry::find(int fd) {
    auto it = clients_.find(fd);

    if (it == clients_.end()) {
        return nullptr;
    }

    return &it->second;
}

std::size_t ConnectionRegistry::size() const {
    return clients_.size();
}

std::vector<int> ConnectionRegistry::fds() const {
    std::vector<int> result;
    result.reserve(clients_.size());

    for (const auto& client : clients_) {
        result.push_back(client.first);
    }

    return result;
}
