#pragma once

#include <memory>
#include <variant>

#include "config/ServerConfig.hpp"
#include "http/CgiRequest.hpp"
#include "http/ConnectionInfo.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

using HandlerResult = std::variant<HttpResponse, CgiRequest>;

struct RequestPlan;

class RequestDispatcher {
public:
    static std::unique_ptr<RequestPlan> select(HttpRequest request, const ServerConfig& server);
    static HandlerResult dispatch(RequestPlan plan, const ConnectionInfo& connectionInfo);
};
