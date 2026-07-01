#pragma once

#include "config/Configuration.hpp"
#include "net/SocketManager.hpp"

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
    //copy of Config struct stored locally
    Configuration config_;


public:
    /*
    memory and fd cleanup is handled inside the SocketManager, ServerSocket and Socket
    classes, so default destructor will work here
    */
    TcpServer() = default;
    ~TcpServer() = default;

    /*
    Initialization: Parsed config blocks contained in main config struct
    are passed to SocketManager, which creates,
    binds and sets sockets to listen mode.
    */
    void setupServers(const Configuration& config);

    /*
    Called once by EventLoop at startup, to get list of listening FDs to
    register with Poller
    */
    std::vector<int> getListeningFds() const;

    /*
    Used by EventLoop to:
    1. check server's client_max_body_size
    2. find root of the server's file system
    3. check for custom error pages
    */
    const ServerConfig* getServerConfigByPort(int port) const;

    struct ClientInfo {
        int fd;
        std::string ip;
        int port; // Port on client's machine
        int serverPort; // Port on our machine (80, 443 etc)
    };

    /*
    Called by EventLoop when Poller triggers a POLLIN on a listening port.
    Finds the corresponding ServerSocket object and tells it to accept the new client.
    Obtains client info from the ServerSocket and returns it as a ClientInfo object.
    */
    ClientInfo acceptClient(int listenFd);
};
