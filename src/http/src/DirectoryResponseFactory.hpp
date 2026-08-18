#pragma once

#include <filesystem>

#include "ResolvedRoute.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

class DirectoryResponseFactory {
public:
    static HttpResponse create(
        const std::filesystem::path& directoryPath,
        const std::filesystem::path& root,
        const HttpRequest& request,
        const ResolvedRoute& route
    );
};
