#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "config/ServerConfig.hpp"
#include "http/CgiRequest.hpp"
#include "http/ConnectionInfo.hpp"
#include "http/HttpParser.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpSerializer.hpp"
#include "http/HttpStatus.hpp"
#include "http/ResponseBody.hpp"

class HttpSession {
public:
    struct Read {};
    struct Close {};
    struct Write {
        std::string bytes;
    };
    using Action = std::variant<Read, Write, Close, CgiRequest>;

    HttpSession(const ServerConfig& server, ConnectionInfo info);

    Action receive(std::string_view bytes);
    Action emit();
    Action respond(HttpResponse response);
    Action fail(HttpStatus status);
    Action expire();

private:
    enum class Phase { Reading, Responding };

    Action start(HttpResponse response);
    Action finish();
    bool append(std::string& bytes);

    const ServerConfig& server_;
    ConnectionInfo info_;
    HttpParser parser_;
    Phase phase_ = Phase::Reading;
    HttpSerializer::Framing framing_;
    std::optional<ResponseBody> body_;
};
