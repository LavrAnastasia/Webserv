#pragma once

#include "http/HttpSession.hpp"
#include "net/Connection.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class ConnectionRegistry {
public:
    struct Client {
        Client(int fd, const ServerConfig& server, const ConnectionInfo& info);

        Connection connection;
        HttpSession session;
    };

    // called by event loop when a new client connects
    void addConnection(int fd, const std::string& ip, std::uint16_t serverPort, const ServerConfig* config);

    // called by event loop to clean up disconnected client
    void removeConnection(int fd);

    Client* find(int fd);
    std::size_t size() const;
    std::vector<int> fds() const;

private:
    std::unordered_map<int, Client> clients_;
};
