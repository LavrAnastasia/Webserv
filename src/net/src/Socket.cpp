#include "net/Socket.hpp"

#include <fcntl.h> // fcntl(), F_GETFL, F_SETFL, O_NONBLOCK
#include <stdexcept>

Socket::Socket() : fd_(-1) {
}

Socket::~Socket() {
    if (isValidFd()) {
        close(fd_);
    }
}

bool Socket::isValidFd() const {
    return fd_ >= 0;
}

void Socket::setFd(int fd) {
    fd_ = fd;
}

int Socket::getFd() const {
    return fd_;
}

void Socket::setNonBlocking() {
    if (!isValidFd()) {
        return;
    }

    int flags = fcntl(fd_, F_GETFL, 0); //return all flags active on fd_

    if (flags == -1) {
        throw std::runtime_error("NetError: Failed to get socket flags.");
    }

    // try to set non-block flag
    if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("NetError: Failed to set socket to non-blocking.");
    }
}
