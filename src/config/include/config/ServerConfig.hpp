#pragma once

#include <filesystem>
#include <unordered_map>
#include <vector>

#include "config/ListenConfig.hpp"
#include "config/LocationConfig.hpp"

struct ServerConfig {
    std::vector<ListenConfig> listen;

    std::filesystem::path root;
    std::string index;
    std::size_t clientMaxBodySize;

    std::unordered_map<int, std::filesystem::path> errorPages;

    std::vector<LocationConfig> locations;
};
