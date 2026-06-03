#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "config/CgiConfig.hpp"
#include "config/RedirectConfig.hpp"
#include "config/UploadConfig.hpp"
#include "http/HttpMethod.hpp"

struct LocationConfig {
    std::string path;

    std::filesystem::path root;
    std::string index;
    std::size_t clientMaxBodySize;

    std::vector<HttpMethod> allowedMethods;

    bool autoindex;

    std::optional<RedirectConfig> redirect;
    std::optional<UploadConfig> upload;
    std::optional<CgiConfig> cgi;
};
