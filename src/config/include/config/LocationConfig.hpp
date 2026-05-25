#pragma once

#include <filesystem>
#include <optional>
#include <string>

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

struct CgiConfig {
    std::string extension;
    std::filesystem::path interpreter;
};

struct UploadConfig {
    std::filesystem::path uploadPath;
};

struct RedirectConfig {
    int statusCode;
    std::string target;
};
