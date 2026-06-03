#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config/CgiConfig.hpp"
#include "config/ListenConfig.hpp"
#include "config/RedirectConfig.hpp"
#include "config/UploadConfig.hpp"
#include "http/HttpMethod.hpp"


class ConfigDecoder {
public:
    static std::string decodeLocationPath(const std::vector<std::string>& arguments);
    static ListenConfig decodeListen(const std::vector<std::string>& arguments);
    static std::filesystem::path decodeRoot(const std::vector<std::string>& arguments);
    static std::string decodeIndex(const std::vector<std::string>& arguments);
    static std::size_t decodeClientMaxBodySize(const std::vector<std::string>& arguments);
    static std::unordered_map<int, std::filesystem::path> decodeErrorPage(const std::vector<std::string>& arguments);
    static std::unordered_set<HttpMethod> decodeMethods(const std::vector<std::string>& values);
    static bool decodeAutoIndex(std::string_view value);
    static RedirectConfig decodeRedirect(const std::vector<std::string>& arguments);
    static UploadConfig decodeUpload(const std::vector<std::string>& arguments);
    static CgiConfig decodeCgi(const std::vector<std::string>& arguments);
};
