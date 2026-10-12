#include "net/Connection.hpp"

#include <sys/socket.h>
#include <utility>

Connection::Connection(int fd, ConnectionInfo info)
    : info_(std::move(info)), lastActivity_(std::chrono::steady_clock::now()) {
    setFd(fd);
    setNonBlocking();
    setCloseOnExec();
}

std::optional<std::string_view> Connection::receive() {
    const ssize_t received = ::recv(getFd(), readBuffer_.data(), readBuffer_.size(), 0);

    // host disconnected or the socket failed after poll reported it readable
    if (received <= 0) {
        return std::nullopt;
    }

    lastActivity_ = std::chrono::steady_clock::now();
    return std::string_view(readBuffer_.data(), static_cast<std::size_t>(received));
}

void Connection::queue(std::string bytes) {
    if (pending() == 0) {
        sendBuffer_ = std::move(bytes);
        sendOffset_ = 0;
    } else {
        sendBuffer_ += bytes;
    }

    lastActivity_ = std::chrono::steady_clock::now();
}

bool Connection::send() {
    if (pending() == 0) {
        return true;
    }

    const ssize_t sent = ::send(getFd(), sendBuffer_.data() + sendOffset_, pending(), 0);

    // client disconnected or the socket failed after poll reported it writable
    if (sent <= 0) {
        return false;
    }

    sendOffset_ += static_cast<std::size_t>(sent);

    if (pending() == 0) {
        std::string().swap(sendBuffer_);
        sendOffset_ = 0;
    }

    lastActivity_ = std::chrono::steady_clock::now();
    return true;
}

std::size_t Connection::pending() const {
    return sendBuffer_.size() - sendOffset_;
}

bool Connection::isAlive() const {
    char byte;

    return ::recv(getFd(), &byte, 1, MSG_PEEK) > 0;
}

bool Connection::hasTimedOut(std::chrono::steady_clock::time_point currentTime, int timeoutSeconds) const {
    return std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastActivity_).count() > timeoutSeconds;
}
