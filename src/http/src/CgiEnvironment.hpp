#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "http/HttpRequest.hpp"

struct ConnectionInfo;

class CgiEnvironment {
public:
    static std::vector<std::string>
    build(const HttpRequest& request, const std::filesystem::path& script, const ConnectionInfo& connectionInfo);
};
