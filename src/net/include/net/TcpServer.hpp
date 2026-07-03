#pragma once

#include "config/Configuration.hpp"
#include "net/SocketManager.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/*
    The TCP server owns the SocketManager, sets up listening ports, and routes
    incoming connection requests to the appropriate ServerSocket.
*/

class TcpServer {
private:
    //mapping of ports to FDs is handled inside the SocketManager's ServerSocket objects
    SocketManager socketManager_;
    //main owns Config struct -> avoid deep copy and only store reference here
    const Configuration& config_;


public:
    /*
    memory and fd cleanup is handled inside the SocketManager, ServerSocket and Socket
    classes
    */
    explicit TcpServer(const Configuration& config);
    ~TcpServer() = default;

    /*
    Called once by EventLoop at startup, to get list of listening FDs to
    register with Poller
    */
    std::vector<int> getListeningFds() const;

    const ServerConfig* getConfigForFd(int listenFd) const;

    struct ClientInfo {
        int fd;
        std::string ip;
        std::uint16_t port; // Port on client's machine
        std::uint16_t serverPort; // Port on our machine (80, 443 etc)
    };

    /*
    Called by EventLoop when Poller triggers a POLLIN on a listening port.
    Finds the corresponding ServerSocket object and tells it to accept the new client.
    Obtains client info from the ServerSocket and returns it as a ClientInfo object.
    */
    std::optional<TcpServer::ClientInfo> acceptClient(int listenFd);
};
