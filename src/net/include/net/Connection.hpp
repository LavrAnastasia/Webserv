#pragma once

#include "http/ConnectionInfo.hpp"
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

class Connection : public Socket {
private:
    ConnectionInfo info_;
    HttpParser parser_;
    std::string sendBuffer_;
    std::size_t sendOffset_ = 0;
    std::optional<ResponseBody> body_;
    bool bodyFailed_ = false;
    const ServerConfig& serverConfig_;
    std::chrono::steady_clock::time_point lastActivity_;
    bool shouldClose_;
    bool headersOnly_ = false;

    bool refill();

public:
    enum class SendResult { Ok, BodyError, SocketError };

    Connection(int fd, const std::string& ip, std::uint16_t serverPort, const ServerConfig& config);

    const ConnectionInfo& info() const { return info_; }

    //get server configuration to access rule sets
    const ServerConfig& getServerConfig() const { return serverConfig_; }

    void setShouldClose(bool state) { shouldClose_ = state; }
    bool shouldClose() const { return shouldClose_; }
    void setHeadersOnly(bool state);
    void resetParser() { parser_.reset(); }

    // used by EventLoop to determine when to switch between POLLOUT and POLLIN
    bool isSendComplete() const;

    void setResponse(HttpResponse response);

    //called by server when POLLIN detected -reads raw bytes from socket -> HttpParser
    std::optional<ParseResult> receiveRequest();

    SendResult sendResponse();
    bool isAlive() const;
    bool hasTimedOut(std::chrono::steady_clock::time_point currentTime, int timeoutSeconds) const;
};
