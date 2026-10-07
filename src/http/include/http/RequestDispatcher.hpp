#pragma once

#include <variant>

#include "config/ServerConfig.hpp"
#include "http/CgiRequest.hpp"
#include "http/ConnectionInfo.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

using HandlerResult = std::variant<HttpResponse, CgiRequest>;

class RequestDispatcher {
public:
    static HandlerResult
    dispatch(const HttpRequest& request, const ServerConfig& server, const ConnectionInfo& connectionInfo);
    static HttpResponse fail(HttpStatus status, const ServerConfig& server);
};
