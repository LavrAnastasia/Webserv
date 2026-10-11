#pragma once

#include "http/ConnectionInfo.hpp"
#include "http/HttpSerializer.hpp"
#include "net/Socket.hpp"

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

class Connection : public Socket {
private:
    ConnectionInfo info_;
    std::array<char, 4096> readBuffer_{};
    std::string sendBuffer_;
    std::size_t sendOffset_ = 0;
    std::optional<ResponseBody> body_;
    bool bodyFailed_ = false;
    std::chrono::steady_clock::time_point lastActivity_;

    bool refill();

public:
    enum class SendResult { Ok, BodyError, SocketError };

    Connection(int fd, ConnectionInfo info);

    const ConnectionInfo& info() const { return info_; }

    // called on POLLIN: reads raw bytes from the socket, nullopt when the client is gone
    std::optional<std::string_view> receive();
    void setResponse(HttpSerializer::Output output);
    SendResult sendResponse();

    // used by EventLoop to determine when to switch between POLLOUT and POLLIN
    bool isSendComplete() const;
    bool isAlive() const;
    bool hasTimedOut(std::chrono::steady_clock::time_point currentTime, int timeoutSeconds) const;
};
