#include "Connection.hpp"


Connection::Connection(int fd, const std::string& ip, int port) {
}
Connection::~Connection() {
}

bool Connection::hasCompleteHeaders() const {
}

// called by server, consumes bytes which were parsed by http parser
void Connection::consumeReadBuffer(size_t bytes) {
}

// called by server, appends HTTP response string to writeBuffer_
void Connection::appendResponse(const std::string& response) {
}
//called by server when status == POLLIN, calls recv() and appends to readBuffer_
bool Connection::read() {
}

//called by server when status == POLLOUT, calls send() and removes bytes from writeBuffer_
bool Connection::write() {
}
