#pragma once

#include <filesystem>

#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

class AutoindexResponseFactory {
public:
    static HttpResponse create(const std::filesystem::path& directoryPath, const HttpRequest& request);
};
