#include <filesystem>

#include "HttpSyntax.hpp"
#include "Router.hpp"

namespace {
    const std::string kPathSeparator(1, Http::Syntax::PathPrefix);

    bool isLocationMatch(const std::string& requestPath, const std::string& locationPath) {
        if (locationPath == kPathSeparator) {
            return true;
        }

        if (requestPath == locationPath) {
            return true;
        }

        return requestPath.starts_with(locationPath + kPathSeparator);
    }

    const LocationConfig* findLocation(const std::string& requestPath, const std::vector<LocationConfig>& locations) {
        const LocationConfig* result = nullptr;

        for (const LocationConfig& location : locations) {
            if (!isLocationMatch(requestPath, location.path)) {
                continue;
            }

            if (result == nullptr || location.path.size() > result->path.size()) {
                result = &location;
            }
        }

        return result;
    }

    std::optional<CgiConfig> resolveCgi(const std::string& requestPath, const LocationConfig& location) {
        const std::string extension = std::filesystem::path(requestPath).extension().string();

        if (extension.empty()) {
            return std::nullopt;
        }

        const auto cgiIt = location.cgi.find(extension);

        if (cgiIt == location.cgi.end()) {
            return std::nullopt;
        }

        return cgiIt->second;
    }
} // namespace

std::optional<ResolvedRoute> Router::resolve(const HttpRequest& request, const ServerConfig& server) {
    if (request.path.empty() || request.path.front() != Http::Syntax::PathPrefix) {
        return std::nullopt;
    }

    const LocationConfig* location = findLocation(request.path, server.locations);

    if (location == nullptr) {
        return std::nullopt;
    }

    ResolvedRoute route;

    route.locationPath = location->path;
    route.root = location->root.value_or(server.root);
    route.index = location->index.value_or(server.index);
    route.clientMaxBodySize = location->clientMaxBodySize.value_or(server.clientMaxBodySize);
    for (const auto& [code, page] : server.errorPages) {
        route.errorPages.emplace(code, route.root / page.relative_path());
    }

    route.allowedMethods = location->allowedMethods;
    route.autoindex = location->autoindex;
    route.redirect = location->redirect;
    route.upload = location->upload;
    route.cgi = resolveCgi(request.path, *location);

    return route;
}
