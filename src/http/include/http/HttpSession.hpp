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
    struct ReadOutput {};
    struct Close {};
    struct Write {
        std::string bytes;
    };
    using Action = std::variant<Read, ReadOutput, Write, Close, CgiRequest>;
    enum class CgiFailure { Busy, Launch, Output, Timeout };

    HttpSession(const ServerConfig& server, ConnectionInfo info);

    Action receive(std::string_view bytes);
    Action emit();
    Action expire();
    Action receiveOutput(std::string_view bytes);
    Action endOutput();
    Action fail(CgiFailure failure);

private:
    enum class Phase { Reading, Responding };

    Action respond(HttpResponse response);
    Action finish();
    Action fail(HttpStatus status);
    bool append(std::string& bytes);

    const ServerConfig& server_;
    ConnectionInfo info_;
    HttpParser parser_;
    Phase phase_ = Phase::Reading;
    HttpSerializer::Framing framing_;
    std::optional<ResponseBody> body_;
    std::string output_;
};
