#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <variant>

#include "config/ServerConfig.hpp"
#include "http/CgiRequest.hpp"
#include "http/ConnectionInfo.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

using HandlerResult = std::variant<HttpResponse, CgiRequest>;

class RequestHandler;

using DispatchResult = std::variant<HttpResponse, std::unique_ptr<RequestHandler>>;

class RequestDispatcher {
public:
    static DispatchResult dispatch(
        HttpRequest request,
        std::optional<std::size_t> length,
        const ServerConfig& server,
        const ConnectionInfo& connectionInfo
    );
};
