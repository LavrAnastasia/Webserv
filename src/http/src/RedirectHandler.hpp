#pragma once

#include "config/RedirectConfig.hpp"
#include "http/HttpResponse.hpp"

#include "ResolvedRoute.hpp"

class RedirectHandler {
public:
    static HttpResponse handle(const RedirectConfig& redirect, const ResolvedRoute& route);
};
