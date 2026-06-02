#include "ServerSocket.hpp"
#include <fcntl.h> // fcntl(), F_GETFL, F_SETFL, O_NONBLOCK
#include <stdexcept>
#include <sys/socket.h> // socket(), AF_INET, SOCK_STREAM
#include <unistd.h>

ServerSocket::ServerSocket(int port) : port_(port), fd_(-1) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        throw std::runtime_error("NetError: Failed to create socket.");
    }

    address_.sin_family = AF_INET; //external IPv4 address
    address_.sin_port = htons(port_); //convert port_ from machine to server byte order
    address_.sin_addr.s_addr = htonl(INADDR_ANY); //accept connections on any IP

    int opt = 1;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(fd_);
        throw std::runtime_error("NetError: Failed to set SO_REUSEADDR.");
    } //override port's TIME_WAIT, allowing instant reconnection

    if (bind(fd_, (struct sockaddr*)&address_, sizeof(address_)) == -1) {
        close(fd_);
        throw std::runtime_error("NetError: Failed to bind to port.");
    } //bind() can fail if port is already in use, or if permission is denied

    if (listen(fd_, SOMAXCONN) == -1) {
        close(fd_);
        throw std::runtime_error("NetError: Failed to listen on socket.");
    } //set network socket to listen mode, SOMAXCONN = max allowed connection queue size
}

ServerSocket::~ServerSocket() {
    if (fd_ >= 0) {
        close(fd_);
    }
}

int ServerSocket::getFd() const {
    return fd_;
}

int ServerSocket::getPort() const {
    return port_;
}

void ServerSocket::setNonBlocking() {
    int flags = fcntl(fd_, F_GETFL, 0); //return all flags active on fd_
    if (flags == -1) {
        throw std::runtime_error("NetError: Failed to get socket flags.");
    }
    if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("NetError: Failed to set socket to non-blocking.");
    }
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
*/
