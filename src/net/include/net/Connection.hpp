#pragma once

#include "http/HttpParser.hpp"
#include "http/HttpResponse.hpp"
#include "net/Socket.hpp"
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

/*
    The Connection class handles the transport layer of a client session.
    It manages the socket File Descriptor, reads raw bytes into the stateful
    HttpParser, and handles the buffered sending of responses.
*/

//forward declaration sufficient for pointer
struct ServerConfig;

struct ConnectionInfo {
    std::string remoteAddr;
    std::uint16_t serverPort;
};

class Connection : public Socket {
private:
    ConnectionInfo info_;
    HttpParser parser_;
    std::string sendBuffer_;
    std::size_t sendOffset_ = 0;
    std::optional<ResponseBody> body_;
    const ServerConfig& serverConfig_;
    std::chrono::steady_clock::time_point lastActivity_;
    bool shouldClose_;

public:
    Connection(int fd, const std::string& ip, std::uint16_t serverPort, const ServerConfig& config);

    const std::string& getClientIp() const { return info_.remoteAddr; }
    const ConnectionInfo& info() const { return info_; }

    //get server configuration to access rule sets
    const ServerConfig& getServerConfig() const { return serverConfig_; }

    void setShouldClose(bool state) { shouldClose_ = state; }
    bool shouldClose() const { return shouldClose_; }
    void resetParser() { parser_.reset(); }

    // used by EventLoop to determine when to switch between POLLOUT and POLLIN
    bool isSendComplete() const;

    // called by server, serializes the response into sendBuffer_
    void appendResponse(HttpResponse response);

    //called by server when POLLIN detected -reads raw bytes from socket -> HttpParser
    std::optional<ParseResult> receiveRequest();

    bool sendResponse();
    bool hasTimedOut(std::chrono::steady_clock::time_point currentTime, int timeoutSeconds) const;
};
