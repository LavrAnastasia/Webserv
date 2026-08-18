#pragma once

#include <filesystem>

#include "ResolvedRoute.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

class AutoindexResponseFactory {
public:
    static HttpResponse
    create(const std::filesystem::path& directoryPath, const HttpRequest& request, const ResolvedRoute& route);
};
