#include "Connection.hpp"

#include <sys/socket.h>


Connection::Connection(int fd, const std::string& ip, int port)
    : clientIp_(ip), clientPort_(port), currentState_(State::READING) {
    fd_ = fd;
    lastActivity_ = time(nullptr); //set start of timeout timer
}


bool Connection::hasCompleteHeaders() const {
    return readBuffer_.find("\r\n\r\n") != std::string::npos; //find returns npos if not found
}

/*
    -called by server, consumes number of bytes parsed by http parser (starting at index 0)
    -erase automatically manages memory to shift remaining bytes (to start at index 0
    in this case)
*/
void Connection::consumeReadBuffer(size_t bytes) {
    readBuffer_.erase(0, bytes);
}

// called by server, appends HTTP response string to writeBuffer_ and updates state
void Connection::appendResponse(const std::string& response) {
    writeBuffer_.append(response);
    currentState_ = State::WRITING;
}
//called by server when status == POLLIN, calls recv() and appends to readBuffer_
bool Connection::read() {
}

//called by server when status == POLLOUT, calls send() and removes bytes from writeBuffer_
bool Connection::write() {
}
