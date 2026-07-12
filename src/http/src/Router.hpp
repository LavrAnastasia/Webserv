#pragma once

#include <optional>

#include "config/ServerConfig.hpp"
#include "http/HttpRequest.hpp"

#include "ResolvedRoute.hpp"

class Router {
public:
    std::optional<ResolvedRoute> resolve(const HttpRequest& request, const ServerConfig& server) const;
};
