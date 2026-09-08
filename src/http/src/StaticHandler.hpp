#pragma once

#include "ResolvedRoute.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

class StaticHandler {
public:
    static HttpResponse handle(const HttpRequest& request, const ResolvedRoute& route);
};
