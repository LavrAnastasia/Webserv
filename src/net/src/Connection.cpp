#include "net/Connection.hpp"

#include <sys/socket.h>


Connection::Connection(int fd, const std::string& ip, const ServerConfig& config)
    : clientIp_(ip), serverConfig_(config), lastActivity_(std::chrono::steady_clock::now()), shouldClose_(false) {
    setFd(fd);
    setNonBlocking();
}

// called by server, appends HTTP response string to sendBuffer_ and updates state
void Connection::appendResponse(const std::string& response) {
    sendBuffer_.append(response);
}

/*
    called by server when status == POLLIN, calls recv() and appends to parser_'s
    internal buffer

    recv function signature: ssize_t recv(int sockfd, void *buf, size_t len, int flags);
    0 for flags is the default

    NOTE: Aggregate Initialization syntax:
    return { ParseStatus::NeedMoreData, std::nullopt }; creates and initializes
    a struct of the return type, with the arguments given
*/
ParseResult Connection::receiveRequest() {
    char buffer[4096];
    ssize_t bytesReceived = recv(getFd(), buffer, sizeof(buffer), 0);

    // treat all negative returns as no data, try again next loop
    if (bytesReceived < 0) {
        return {ParseStatus::NeedMoreData, std::nullopt};
    }

    // host disconnected, return status that triggers cleanup
    if (bytesReceived == 0) {
        return {ParseStatus::ConnectionClosed, std::nullopt};
    }

    lastActivity_ = std::chrono::steady_clock::now(); // update timeout timer

    // recv return > 0 indicates number of bytes successfully received
    return parser_.append(buffer, bytesReceived);
}

/*
    called by server when status == POLLOUT, calls send() and removes bytes from sendBuffer_
    send function signature: ssize_t send(int sockfd, const void *buf, size_t len, int flags);
*/
bool Connection::sendResponse() {
    //early exit if buffer is empty
    if (sendBuffer_.empty()) {
        return true;
    }

    ssize_t bytesSent = send(getFd(), sendBuffer_.data(), sendBuffer_.length(), 0);

    //treat all negative returns as OS buffer full, try again
    if (bytesSent < 0) {
        return true;
    }

    // return false if client disconnected during send
    if (bytesSent == 0) {
        return false;
    }

    //bytes successfully sent, remove them from outgoing buffer
    sendBuffer_.erase(0, bytesSent);

    //update timeout timer whenever bytes sent: prevent timeout during large transfers
    lastActivity_ = std::chrono::steady_clock::now();
    return true;
}

bool Connection::hasTimedOut(std::chrono::steady_clock::time_point currentTime, int timeoutSeconds) const {
    return std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastActivity_).count() > timeoutSeconds;
}
