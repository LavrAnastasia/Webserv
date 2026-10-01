#include "net/Socket.hpp"

#include <stdexcept>

Socket::Socket() = default;

Socket::~Socket() = default;

bool Socket::isValidFd() const {
    return fd_.isOpen();
}

void Socket::setFd(int fd) {
    fd_ = FileDescriptor(fd);
}

int Socket::getFd() const {
    return fd_.get();
}

void Socket::setNonBlocking() {
    if (!fd_.setNonBlocking()) {
        throw std::runtime_error("NetError: Failed to set socket to non-blocking.");
    }
}

void Socket::setCloseOnExec() {
    if (!fd_.setCloseOnExec()) {
        throw std::runtime_error("NetError: Failed to set socket to close-on-exec.");
    }
}
