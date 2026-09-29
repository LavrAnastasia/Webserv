#pragma once

#include "http/HttpRequest.hpp"
#include "http/RequestHandler.hpp"

#include "ResolvedRoute.hpp"

class CgiHandler {
public:
    static HandlerResult handle(const HttpRequest& request, const ResolvedRoute& route);
};
