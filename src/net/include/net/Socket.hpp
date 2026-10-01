#pragma once

#include "fs/FileDescriptor.hpp"

class Socket {
private:
    FileDescriptor fd_;

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
    void setCloseOnExec();
};
