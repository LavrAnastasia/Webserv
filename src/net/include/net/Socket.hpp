#pragma once

#include <unistd.h>

class Socket {
private:
    int fd_;

protected:
    bool isValidFd() const;
    void setFd(int fd);

public:
    Socket();
    virtual ~Socket();

    // disable accidental fd copying
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    int getFd() const;
    void setNonBlocking();
};
