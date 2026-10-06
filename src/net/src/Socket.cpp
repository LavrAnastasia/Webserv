#include "net/Socket.hpp"

#include <cerrno>
#include <system_error>

Socket::Socket() = default;

Socket::~Socket() = default;

void Socket::setFd(int fd) {
    fd_ = FileDescriptor(fd);
}

int Socket::getFd() const {
    return fd_.get();
}

void Socket::setNonBlocking() {
    if (!fd_.setNonBlocking()) {
        throw std::system_error(errno, std::generic_category(), "fcntl(O_NONBLOCK)");
    }
}

void Socket::setCloseOnExec() {
    if (!fd_.setCloseOnExec()) {
        throw std::system_error(errno, std::generic_category(), "fcntl(FD_CLOEXEC)");
    }
}
