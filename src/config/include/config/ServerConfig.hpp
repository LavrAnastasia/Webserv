#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "config/ListenConfig.hpp"
#include "config/LocationConfig.hpp"

struct ServerConfig {
    std::vector<ListenConfig> listen;

    std::filesystem::path root;
    std::string index = "index.html";
    std::size_t clientMaxBodySize = 1 * 1024 * 1024;

    std::unordered_map<int, std::filesystem::path> errorPages;

    std::vector<LocationConfig> locations;
};
