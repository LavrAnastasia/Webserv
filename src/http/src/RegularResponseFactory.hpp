#pragma once

#include "ResolvedRoute.hpp"
#include "http/HttpResponse.hpp"

class RegularResponseFactory {
public:
    static HttpResponse create(const std::filesystem::path& path, const ResolvedRoute& route);
};
