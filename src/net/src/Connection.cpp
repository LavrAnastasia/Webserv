#include "net/Connection.hpp"

#include <sys/socket.h>


Connection::Connection(int fd, const std::string& ip, const ServerConfig& config)
    : clientIp_(ip), serverConfig_(config), lastActivity_(std::chrono::steady_clock::now()) {
    setFd(fd);
    setNonBlocking();
}


/*
    -called by server, consumes number of bytes parsed by http parser (starting at index 0)
    -erase automatically manages memory to shift remaining bytes (to start at index 0
    in this case)
*/
void Connection::consumeReceiveBuffer(size_t bytes) {
    receiveBuffer_.erase(0, bytes);
}

// called by server, appends HTTP response string to sendBuffer_ and updates state
void Connection::appendResponse(const std::string& response) {
    sendBuffer_.append(response);
}

/*
    called by server when status == POLLIN, calls recv() and appends to receiveBuffer_
    recv function signature: ssize_t recv(int sockfd, void *buf, size_t len, int flags);
    0 for flags is the default
*/
bool Connection::receiveRequest() {
    char buffer[4096];
    ssize_t bytesReceived = recv(getFd(), buffer, sizeof(buffer), 0);

    // treat all negative returns as no data, try again next loop
    if (bytesReceived < 0) {
        return true;
    }

    // return false for graceful close (EOF), EventLoop handles cleanup
    if (bytesReceived == 0) {
        return false;
    }

    // recv return > 0 indicates number of bytes successfully received
    receiveBuffer_.append(buffer, bytesReceived);
    lastActivity_ = std::chrono::steady_clock::now(); // update timeout timer
    return true;
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
