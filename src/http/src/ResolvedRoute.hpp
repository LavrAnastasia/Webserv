#pragma once

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>

#include "config/CgiConfig.hpp"
#include "config/RedirectConfig.hpp"
#include "config/UploadConfig.hpp"
#include "http/HttpMethod.hpp"

struct ResolvedRoute {
    std::string locationPath;

    std::filesystem::path root;
    std::string index;
    std::size_t clientMaxBodySize;

    std::set<HttpMethod> allowedMethods;
    bool autoindex = false;

    std::optional<RedirectConfig> redirect;
    std::optional<UploadConfig> upload;
    std::optional<CgiConfig> cgi;

    std::unordered_map<int, std::filesystem::path> errorPages;
};
