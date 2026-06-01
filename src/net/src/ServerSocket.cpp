#include "ServerSocket.hpp"
#include <stdexcept>
#include <sys/socket.h> // socket(), AF_INET, SOCK_STREAM
#include <unistd.h>

ServerSocket::ServerSocket(int port) : port_(port), fd_(-1) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        throw std::runtime_error("NetError: Failed to create socket.");
    }
    address_.sin_family = AF_INET;
    address_.sin_port = htons(port_); //convert port_ from machine to server byte order
    address_.sin_addr.s_addr = INADDR_ANY; //accept connections on any IP
}

/*
    The anatomy of socket(AF_INET, SOCK_STREAM, 0):
    Argument 1 (domain) AF_INET = Address Family: INTernet (external IPv4 internet)
    Argument 2 (type) SOCK_STREAM = Socket Stream: reliable two-way continuous
    pipeline, data guaranteed to arrive in order and without missing pieces
    Argument 3 (protocol) 0 = default protocol - for AF_INET + SOCK_STREAM this defaults
    to TCP
*/
