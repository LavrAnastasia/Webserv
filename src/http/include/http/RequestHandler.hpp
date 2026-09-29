#pragma once

#include "config/ServerConfig.hpp"
#include "http/CgiRequest.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

using HandlerResult = std::variant<HttpResponse, CgiRequest>;

class RequestHandler {
public:
    static HttpResponse handle(const HttpRequest& request, const ServerConfig& server);
    static HttpResponse reject(HttpStatus status, const ServerConfig& server);
};
