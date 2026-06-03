#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "config/CgiConfig.hpp"
#include "config/RedirectConfig.hpp"
#include "config/UploadConfig.hpp"
#include "http/HttpMethod.hpp"

struct LocationConfig {
    std::string path;

    std::filesystem::path root;
    std::string index;
    std::optional<std::size_t> clientMaxBodySize;

    std::unordered_set<HttpMethod> allowedMethods;

    bool autoindex;

    std::optional<RedirectConfig> redirect;
    std::optional<UploadConfig> upload;
    std::unordered_map<std::string, CgiConfig> cgi;
};
