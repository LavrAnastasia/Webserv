#pragma once

#include <unistd.h>

class Socket {
protected:
    int fd_;

public:
    Socket() : fd_(-1) {}

    virtual ~Socket() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    // disable accidental fd copying
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    int getFd() const { return fd_; }
    void setNonBlocking();
};
