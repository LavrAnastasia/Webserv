#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct CgiRequest {
    std::filesystem::path interpreter;
    std::filesystem::path script;
    std::vector<std::string> env;
    std::string body;
};
