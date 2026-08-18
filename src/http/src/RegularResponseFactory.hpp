#pragma once

#include <array>
#include <cerrno>
#include <fstream>
#include <ios>
#include <system_error>
#include <utility>

#include "ResolvedRoute.hpp"
#include "http/HttpResponse.hpp"

class RegularResponseFactory {
public:
    static HttpResponse create(const std::filesystem::path& path, const ResolvedRoute& route);
};
