#include "net/ServerSocket.hpp"

#include <arpa/inet.h> // inet_pton()
#include <stdexcept>
#include <string>
#include <sys/socket.h> // socket(), AF_INET, SOCK_STREAM
#include <unistd.h>

ServerSocket::ServerSocket(const std::string& host, std::uint16_t port) : port_(port) {
    int newFd = socket(AF_INET, SOCK_STREAM, 0);
    if (newFd < 0) {
        throw std::runtime_error("NetError: Failed to create socket.");
    }

    setFd(newFd);

    socketAddress_.sin_family = AF_INET; //external IPv4 address
    socketAddress_.sin_port = htons(port_); //convert port_ from machine to server byte order

    if (host.empty() || host == "0.0.0.0") {
        socketAddress_.sin_addr.s_addr = htonl(INADDR_ANY); //accept connections on any IP
    } else {
        // convert string to uint32_t to be usable by OS
        if (inet_pton(AF_INET, host.c_str(), &socketAddress_.sin_addr) <= 0) {
            throw std::runtime_error("NetError: Invalid host IP address: " + host);
        }
    }
    int opt = 1;
    if (setsockopt(getFd(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        throw std::runtime_error("NetError: Failed to set SO_REUSEADDR.");
    } //override port's TIME_WAIT, allowing instant reconnection

    if (bind(getFd(), (struct sockaddr*)&socketAddress_, sizeof(socketAddress_)) == -1) {
        throw std::runtime_error("NetError: Failed to bind to port.");
    } //bind() can fail if port is already in use, or if permission is denied

    if (listen(getFd(), SOMAXCONN) == -1) {
        throw std::runtime_error("NetError: Failed to listen on socket.");
    } //set network socket to listen mode, SOMAXCONN = max allowed connection queue size
}

std::uint16_t ServerSocket::getPort() const {
    return port_;
}

int ServerSocket::acceptConnection(std::string& clientIp, uint16_t& clientPort) {
    // data for accept() to fill out
    struct sockaddr_in clientAddress;
    socklen_t clientLen = sizeof(clientAddress);

    // OS fills out argument variables with caller's actual info
    int clientFd = accept(getFd(), (struct sockaddr*)&clientAddress, &clientLen);

    if (clientFd < 0) {
        return -1;
    }

    // convert binary IP address into readable string and pass it outside of function by reference
    clientIp = inet_ntoa(clientAddress.sin_addr);

    // convert port from network byte order to normal and pass outside of function by reference
    clientPort = ntohs(clientAddress.sin_port);

    // return new fd for each accepted connection
    return clientFd;
}


/*
    The anatomy of socket(AF_INET, SOCK_STREAM, 0):
    Argument 1 (domain) AF_INET = Address Family: INTernet (external IPv4 internet)
    Argument 2 (type) SOCK_STREAM = Socket Stream: reliable two-way continuous
    pipeline, data guaranteed to arrive in order and without missing pieces
    Argument 3 (protocol) 0 = default protocol - for AF_INET + SOCK_STREAM this defaults
    to TCP

    htonl(INADDR_ANY) is functionally identical to INADDR_ANY, since the macro evaluates
    to 0, which represents the address 0.0.0.0, which is identical whether byte order
    is flipped or not. Here it's used for explicitness

    SO_REUSEADDR overrides a given port's TIME_WAIT, allowing instant reconnection
    in case of server being killed with CTRL + C for example

    setsockopt arguments:
    1. TARGET (which socket we are modifying): fd_
    2. LEVEL (TCP, IP, socket?): SOL_SOCKET
    3. SETTING NAME: SO_REUSEADDR
    4. VALUE (what to change setting to?) NOTE: this function is ancient C, and doesn't
    have booleans!
    5. SIZE OF VALUE: how many bytes to read from memory address pointed to by VALUE

    bind() takes a generic sockaddr dummy struct as an input, allowing support for different
    address types (IPv4, IPv6, Bluetooth etc)

    fcntl() is used to read or change internal settings for open files or sockets
    arguments:
    1. TARGET
    2. COMMAND: F_GETFL (return settings packed into a single int variable)
                F_SETFL (change internal state of target)
    3. INPUT:   a) unused when only getting data
                b) flags | O_NONBLOCK = bitwise merge, which sets specific bit of nonblock flag to 1

    inet_pton(AF_INET, host.c_str(), &socketAddress_.sin_addr) converts a string address to uint32_t
    1. CONVERSION RULE (address type): AF_INET = IPv4
    2. INPUT: host.c_str() converts a std::string object to a c-style string
    3. OUTPUT: &socketAddress_.sin_addr = location of binary IP address, used by the OS

    NOTE:
*/
