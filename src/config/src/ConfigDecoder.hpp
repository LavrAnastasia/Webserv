#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "config/CgiConfig.hpp"
#include "config/ListenConfig.hpp"
#include "config/RedirectConfig.hpp"
#include "config/UploadConfig.hpp"
#include "http/HttpMethod.hpp"


class ConfigDecoder {
public:
    static std::string decodeLocationPath(std::string_view value);
    static ListenConfig decodeListen(std::string_view value);
    static std::filesystem::path decodeRoot(std::string_view value);
    static std::string decodeIndex(std::string_view value);
    static std::size_t decodeClientMaxBodySize(std::string_view value);
    static std::unordered_map<int, std::filesystem::path> decodeErrorPage(const std::vector<std::string>& values);
    static std::set<HttpMethod> decodeMethods(const std::vector<std::string>& values);
    static bool decodeAutoIndex(std::string_view value);
    static RedirectConfig decodeRedirect(std::string_view statusCode, std::string_view target);
    static UploadConfig decodeUpload(std::string_view value);
    static CgiConfig decodeCgi(std::string_view extension, std::string_view interpreter);
};
