#pragma once

#include <string_view>
#include <variant>

#include "config/ServerConfig.hpp"
#include "http/CgiRequest.hpp"
#include "http/ConnectionInfo.hpp"
#include "http/HttpParser.hpp"
#include "http/HttpSerializer.hpp"
#include "http/HttpStatus.hpp"

class HttpSession {
public:
    struct Read {};
    struct Close {};
    using Action = std::variant<Read, Close, HttpSerializer::Output, CgiRequest>;

    HttpSession(const ServerConfig& server, ConnectionInfo info);

    Action receive(std::string_view bytes);
    Action finish();
    Action expire();
    HttpSerializer::Output serialize(HttpResponse response);
    HttpSerializer::Output fail(HttpStatus status);

private:
    enum class Phase { Reading, Responding };

    const ServerConfig& server_;
    ConnectionInfo info_;
    HttpParser parser_;
    Phase phase_ = Phase::Reading;
    HttpSerializer::Framing framing_;
};
