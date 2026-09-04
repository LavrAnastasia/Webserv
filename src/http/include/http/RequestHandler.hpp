#pragma once

#include "config/ServerConfig.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

class RequestHandler {
public:
    static HttpResponse handle(const HttpRequest& request, const ServerConfig& server);
};
