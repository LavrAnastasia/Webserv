#pragma once

#include <filesystem>
#include <string>

struct CgiConfig {
    std::string extension;
    std::filesystem::path interpreter;
};
