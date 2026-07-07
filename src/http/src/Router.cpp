#include "Router.hpp"

std::optional<ResolvedRoute> Router::resolve(const HttpRequest& request, const ServerConfig& server) const {
    (void)request;
    (void)server;
    return std::nullopt;
}
