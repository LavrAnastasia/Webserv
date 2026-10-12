#pragma once

#include <chrono>
#include <memory>
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

class CgiStream;

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
    using Clock = std::chrono::steady_clock::time_point (*)();

    HttpSession(const ServerConfig& server, ConnectionInfo info);
    HttpSession(const ServerConfig& server, ConnectionInfo info, Clock clock);
    ~HttpSession();

    Action receive(std::string_view bytes);
    Action emit();
    std::optional<Action> expire();
    Action receiveOutput(std::string_view bytes);
    Action endOutput();
    Action fail(CgiFailure failure);

private:
    enum class Phase { Reading, Responding };

    Action respond(HttpResponse response);
    Action finish();
    Action fail(HttpStatus status);
    Action failOutput();
    bool append(std::string& bytes);

    const ServerConfig& server_;
    ConnectionInfo info_;
    Clock clock_;
    HttpParser parser_;
    std::chrono::steady_clock::time_point lastReceived_;
    std::optional<std::chrono::steady_clock::time_point> requestStarted_;
    Phase phase_ = Phase::Reading;
    HttpSerializer::Framing framing_;
    std::optional<ResponseBody> body_;
    std::unique_ptr<CgiStream> cgi_;
    bool receivedOutput_ = false;
    bool startedOutput_ = false;
};
