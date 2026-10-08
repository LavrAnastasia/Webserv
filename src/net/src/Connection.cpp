#include "net/Connection.hpp"

#include "config/ServerConfig.hpp"
#include "http/HttpSerializer.hpp"

#include <sys/socket.h>
#include <utility>

namespace {
    constexpr std::size_t kChunkSize = 64 * 1024;
} // namespace

Connection::Connection(int fd, const std::string& ip, std::uint16_t serverPort, const ServerConfig& config)
    : info_{ip, serverPort}, parser_(config.maxBodySize()), serverConfig_(config),
      lastActivity_(std::chrono::steady_clock::now()), shouldClose_(false) {
    setFd(fd);
    setNonBlocking();
    setCloseOnExec();
}

void Connection::setHeadersOnly(bool state) {
    headersOnly_ = state;
}

void Connection::setResponse(HttpResponse response) {
    auto output = HttpSerializer::serialize(std::move(response), {.close = shouldClose_, .headersOnly = headersOnly_});

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

/*
    called by server when status == POLLIN, calls recv() and appends to parser_'s
    internal buffer

    recv function signature: ssize_t recv(int sockfd, void *buf, size_t len, int flags);
    0 for flags is the default
*/
std::optional<ParseResult> Connection::receiveRequest() {
    char buffer[4096];
    ssize_t bytesReceived = recv(getFd(), buffer, sizeof(buffer), 0);

    // host disconnected or the socket failed after poll reported it readable
    if (bytesReceived <= 0) {
        return std::nullopt;
    }

    lastActivity_ = std::chrono::steady_clock::now(); // update timeout timer

    // recv return > 0 indicates number of bytes successfully received
    return parser_.append(buffer, bytesReceived);
}

bool Connection::isAlive() const {
    char byte;

    return recv(getFd(), &byte, 1, MSG_PEEK) > 0;
}

bool Connection::refill() {
    if (!body_) {
        return true;
    }

    const auto limit = body_->isInMemory() ? static_cast<std::size_t>(body_->size()) : kChunkSize;
    if (!body_->next(sendBuffer_, limit)) {
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

    // Keep initialized storage between file chunks; release it once the response is complete.
    if (isSendComplete()) {
        std::string().swap(sendBuffer_);
        sendOffset_ = 0;
    }

    //update timeout timer whenever bytes sent: prevent timeout during large transfers
    lastActivity_ = std::chrono::steady_clock::now();
    return SendResult::Ok;
}

bool Connection::hasTimedOut(std::chrono::steady_clock::time_point currentTime, int timeoutSeconds) const {
    return std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastActivity_).count() > timeoutSeconds;
}
