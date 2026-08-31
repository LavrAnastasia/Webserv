#pragma once

#include "http/HttpParser.hpp"
#include "net/Socket.hpp"
#include <chrono>
#include <string>

/*
    The Connection class handles the transport layer of a client session.
    It manages the socket File Descriptor, reads raw bytes into the stateful
    HttpParser, and handles the buffered sending of responses.
*/

//forward declaration sufficient for pointer
struct ServerConfig;

class Connection : public Socket {
private:
    std::string clientIp_;
    HttpParser parser_;
    std::string sendBuffer_;
    const ServerConfig& serverConfig_;
    std::chrono::steady_clock::time_point lastActivity_;
    bool shouldClose_;

public:
    Connection(int fd, const std::string& ip, const ServerConfig& config);

    const std::string& getClientIp() const { return clientIp_; }

    //get server configuration to access rule sets
    const ServerConfig& getServerConfig() const { return serverConfig_; }

    void setShouldClose(bool state) { shouldClose_ = state; }
    bool shouldClose() const { return shouldClose_; }
    void resetParser() { parser_ = HttpParser(); }

    // used by EventLoop to determine when to switch between POLLOUT and POLLIN
    bool isSendComplete() const { return sendBuffer_.empty(); }

    // called by server, appends HTTP response string to sendBuffer_
    void appendResponse(const std::string& response);

    //called by server when POLLIN detected -reads raw bytes from socket -> HttpParser
    ParseResult receiveRequest();

    //called by server when status == POLLOUT, calls send() and removes bytes from sendBuffer_
    bool sendResponse();
    bool hasTimedOut(std::chrono::steady_clock::time_point currentTime, int timeoutSeconds) const;
};
