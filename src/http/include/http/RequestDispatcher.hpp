#pragma once

#include <memory>
#include <variant>

#include "config/ServerConfig.hpp"
#include "http/CgiRequest.hpp"
#include "http/ConnectionInfo.hpp"
#include "http/HttpParser.hpp"
#include "http/HttpResponse.hpp"

using HandlerResult = std::variant<HttpResponse, CgiRequest>;

class RequestHandler;

using DispatchResult = std::variant<HttpResponse, std::unique_ptr<RequestHandler>>;

class RequestDispatcher {
public:
    static DispatchResult
    dispatch(HeadersReady headers, const ServerConfig& server, const ConnectionInfo& connectionInfo);
};
