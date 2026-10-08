#include "net/Connection.hpp"

#include "config/ServerConfig.hpp"
#include "http/HttpSerializer.hpp"

#include <sys/socket.h>
#include <utility>


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

void Connection::appendResponse(HttpResponse response) {
    auto output = HttpSerializer::serialize(std::move(response), {.close = shouldClose_, .headersOnly = headersOnly_});

    sendBuffer_ = std::move(output.headers);
    sendOffset_ = 0;
    body_ = std::move(output.body);

    lastActivity_ = std::chrono::steady_clock::now();
}

bool Connection::isSendComplete() const {
    return sendBuffer_.empty() && !body_;
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

bool Connection::sendResponse() {
    if (sendBuffer_.empty()) {
        if (!body_) {
            return true;
        }

        auto chunk = body_->next(64 * 1024);

        if (!chunk) {
            return false;
        }

        sendBuffer_ = std::move(*chunk);
        sendOffset_ = 0;

        if (body_->done()) {
            body_.reset();
        }

        if (sendBuffer_.empty()) {
            return true;
        }
    }

    ssize_t bytesSent = send(getFd(), sendBuffer_.data() + sendOffset_, sendBuffer_.size() - sendOffset_, 0);

    // client disconnected or the socket failed after poll reported it writable
    if (bytesSent <= 0) {
        return false;
    }

    sendOffset_ += static_cast<std::size_t>(bytesSent);

    if (sendOffset_ == sendBuffer_.size()) {
        std::string().swap(sendBuffer_);
        sendOffset_ = 0;
    }

    //update timeout timer whenever bytes sent: prevent timeout during large transfers
    lastActivity_ = std::chrono::steady_clock::now();
    return true;
}

bool Connection::hasTimedOut(std::chrono::steady_clock::time_point currentTime, int timeoutSeconds) const {
    return std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastActivity_).count() > timeoutSeconds;
}
