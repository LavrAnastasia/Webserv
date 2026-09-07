#pragma once

#include "config/RedirectConfig.hpp"
#include "http/HttpResponse.hpp"

class RedirectHandler {
public:
    static HttpResponse handle(const RedirectConfig& redirect);
};
