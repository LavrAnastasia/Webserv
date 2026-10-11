#include "net/Connection.hpp"

#include <sys/socket.h>
#include <utility>

namespace {
    constexpr std::size_t kChunkSize = 64 * 1024;
} // namespace

Connection::Connection(int fd, ConnectionInfo info)
    : info_(std::move(info)), lastActivity_(std::chrono::steady_clock::now()) {
    setFd(fd);
    setNonBlocking();
    setCloseOnExec();
}

std::optional<std::string_view> Connection::receive() {
    const ssize_t received = recv(getFd(), readBuffer_.data(), readBuffer_.size(), 0);

    // host disconnected or the socket failed after poll reported it readable
    if (received <= 0) {
        return std::nullopt;
    }

    lastActivity_ = std::chrono::steady_clock::now();
    return std::string_view(readBuffer_.data(), static_cast<std::size_t>(received));
}

void Connection::setResponse(HttpSerializer::Output output) {
    sendBuffer_.clear();
    sendOffset_ = 0;
    body_ = std::move(output.body);
    bodyFailed_ = !refill();
    sendBuffer_.insert(0, output.headers);

    lastActivity_ = std::chrono::steady_clock::now();
}

bool Connection::isSendComplete() const {
    return sendOffset_ == sendBuffer_.size() && !body_;
}

bool Connection::isAlive() const {
    char byte;

    return recv(getFd(), &byte, 1, MSG_PEEK) > 0;
}

bool Connection::refill() {
    if (!body_) {
        return true;
    }

    if (!body_->next(sendBuffer_, kChunkSize)) {
        return false;
    }

    sendOffset_ = 0;

    if (body_->done()) {
        body_.reset();
    }

    return true;
}

Connection::SendResult Connection::sendResponse() {
    if (bodyFailed_ || (sendOffset_ == sendBuffer_.size() && !refill())) {
        bodyFailed_ = true;
        return SendResult::BodyError;
    }

    if (sendOffset_ == sendBuffer_.size()) {
        return SendResult::Ok;
    }

    ssize_t bytesSent = send(getFd(), sendBuffer_.data() + sendOffset_, sendBuffer_.size() - sendOffset_, 0);

    // client disconnected or the socket failed after poll reported it writable
    if (bytesSent <= 0) {
        return SendResult::SocketError;
    }

    sendOffset_ += static_cast<std::size_t>(bytesSent);

    if (isSendComplete()) {
        std::string().swap(sendBuffer_);
        sendOffset_ = 0;
    }

    lastActivity_ = std::chrono::steady_clock::now();
    return SendResult::Ok;
}

bool Connection::hasTimedOut(std::chrono::steady_clock::time_point currentTime, int timeoutSeconds) const {
    return std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastActivity_).count() > timeoutSeconds;
}
