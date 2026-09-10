#pragma once

#include "ResolvedRoute.hpp"
#include "config/ServerConfig.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpStatus.hpp"

class ErrorResponseFactory {
public:
    static HttpResponse create(HttpStatus status, const ResolvedRoute& route);

    static HttpResponse create(HttpStatus status, const ServerConfig& server);
};
