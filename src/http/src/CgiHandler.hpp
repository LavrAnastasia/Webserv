#pragma once

#include "http/HttpRequest.hpp"
#include "http/RequestDispatcher.hpp"

#include "ResolvedRoute.hpp"

struct ConnectionInfo;

class CgiHandler {
public:
    static HandlerResult handle(HttpRequest request, const ResolvedRoute& route, const ConnectionInfo& connectionInfo);
};
