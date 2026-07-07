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

struct LocationConfig {
    std::string path;

    std::optional<std::filesystem::path> root;
    std::optional<std::string> index;
    std::optional<std::size_t> clientMaxBodySize;

    std::set<HttpMethod> allowedMethods;

    bool autoindex = false;

    std::optional<RedirectConfig> redirect;
    std::optional<UploadConfig> upload;
    std::unordered_map<std::string, CgiConfig> cgi;
};
