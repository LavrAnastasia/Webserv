#include "fs/FileDescriptor.hpp"

#include <utility>

#include <fcntl.h>
#include <unistd.h>

FileDescriptor::FileDescriptor() : fd_(-1) {
}

FileDescriptor::FileDescriptor(int fd) : fd_(fd) {
}

FileDescriptor::~FileDescriptor() {
    close();
}

FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {
}

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = std::exchange(other.fd_, -1);
    }

    return *this;
}

int FileDescriptor::get() const {
    return fd_;
}

bool FileDescriptor::isOpen() const {
    return fd_ >= 0;
}

bool FileDescriptor::close() {
    if (!isOpen()) {
        return true;
    }

    const bool closed = ::close(fd_) == 0;
    fd_ = -1;

    return closed;
}

bool FileDescriptor::setNonBlocking() {
    return ::fcntl(fd_, F_SETFL, O_NONBLOCK) != -1;
}

bool FileDescriptor::setCloseOnExec() {
    return ::fcntl(fd_, F_SETFD, FD_CLOEXEC) != -1;
}
