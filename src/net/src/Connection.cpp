#include "Connection.hpp"

#include <errno.h>
#include <sys/socket.h>


Connection::Connection(int fd, const std::string& ip, int port)
    : clientIp_(ip), clientPort_(port), currentState_(State::RECEIVING) {
    fd_ = fd;
    lastActivity_ = time(nullptr); //set start of timeout timer
}


bool Connection::hasCompleteHeaders() const {
    return receiveBuffer_.find("\r\n\r\n") != std::string::npos; //find returns npos if not found
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
    currentState_ = State::SENDING;
}

/*
    called by server when status == POLLIN, calls recv() and appends to receiveBuffer_
    recv function signature: ssize_t recv(int sockfd, void *buf, size_t len, int flags);
    0 for flags is the default
*/
bool Connection::receiveRequest() {
    char buffer[4096];
    ssize_t bytesReceived = recv(fd_, buffer, sizeof(buffer), 0);

    // indicates a crash or OS kernel "busy" signal -waiting for bytes from client
    if (bytesReceived < 0) {
        // OS kernel socket buffer empty (waiting for input) -> OS kernel returns -11
        // EAGAIN == Error: Try again & EWOULDBLOCK (descriptive) are equivalent
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }
        // crash (client crashed or network cable unplugged)
        currentState_ = State::CLOSED;
        return false;
    }

    // indicates the client has disconnected
    if (bytesReceived == 0) {
        currentState_ = State::CLOSED;
        return false;
    }

    // recv return > 0 indicates number of bytes successfully received
    receiveBuffer_.append(buffer, bytesReceived);
    return true;
}

/*
    called by server when status == POLLOUT, calls send() and removes bytes from sendBuffer_
    send function signature: ssize_t send(int sockfd, const void *buf, size_t len, int flags);
*/
bool Connection::sendResponse() {
    ssize_t bytesSent = send(fd_, sendBuffer_.data(), sendBuffer_.length(), 0);

    if (bytesSent < 0) {
        // OS kernel socket buffer full -> retry
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }
        // crash
        currentState_ = State::CLOSED;
        return false;
    }

    // client disconnected
    if (bytesSent == 0) {
        currentState_ = State::CLOSED;
        return false;
    }

    //bytes successfully sent
    sendBuffer_.erase(0, bytesSent);
    if (sendBuffer_.empty()) {
        currentState_ = State::RECEIVING;
    }
    return true;
}
