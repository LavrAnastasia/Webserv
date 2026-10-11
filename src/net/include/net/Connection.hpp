#pragma once

#include "http/ConnectionInfo.hpp"
#include "net/Socket.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

class Connection : public Socket {
private:
    static constexpr std::size_t kClientHeaderBufferSize = 4 * 1024;

    ConnectionInfo info_;
    std::array<char, kClientHeaderBufferSize> readBuffer_{};
    std::string sendBuffer_;
    std::size_t sendOffset_ = 0;
    std::chrono::steady_clock::time_point lastActivity_;

public:
    Connection(int fd, ConnectionInfo info);

    const ConnectionInfo& info() const { return info_; }

    // called on POLLIN: reads raw bytes from the socket, nullopt when the client is gone
    std::optional<std::string_view> receive();
    void queue(std::string bytes);
    bool send();
    std::size_t pending() const;
    bool isAlive() const;
    bool hasTimedOut(std::chrono::steady_clock::time_point currentTime, int timeoutSeconds) const;
};
