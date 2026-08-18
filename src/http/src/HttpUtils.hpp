#pragma once

#include <string>
#include <string_view>

#include "http/HttpStatus.hpp"
#include <filesystem>
#include <system_error>

namespace Http::Ascii {
    char tolower(char c);
    std::string tolower(const std::string& value);
    std::string trim(const std::string& value);
} // namespace Http::Ascii

namespace Http::TransferCoding {
    constexpr std::string_view Chunked = "chunked";
} // namespace Http::TransferCoding

namespace Http::ContentType {
    constexpr std::string_view Html = "text/html";
}

namespace Http::Server {
    constexpr std::string_view Name = "webserv";
} // namespace Http::Server

namespace Http::Protocol {
    constexpr std::string_view Name = "HTTP";
    constexpr char VersionSeparator = '/';
    constexpr std::string_view Version = "1.1";
} // namespace Http::Protocol

namespace Http {
    bool isNotFoundError(const std::error_code& error);

    HttpStatus statusFromError(const std::error_code& error);

    bool isWithinRoot(const std::filesystem::path& path, const std::filesystem::path& root);
} // namespace Http
