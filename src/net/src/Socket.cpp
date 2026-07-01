#include "net/Socket.hpp"

#include <fcntl.h> // fcntl(), F_GETFL, F_SETFL, O_NONBLOCK
#include <stdexcept>

void Socket::setNonBlocking() {
    if (fd_ < 0) {
        return;
    }

    int flags = fcntl(fd_, F_GETFL, 0); //return all flags active on fd_

    if (flags == -1) {
        throw std::runtime_error("NetError: Failed to get socket flags.");
    }

    if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("NetError: Failed to set socket to non-blocking.");
    }
}
