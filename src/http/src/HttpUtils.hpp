#pragma once

#include <string>
#include <string_view>

namespace Http::Ascii {
    char tolower(char c);
    std::string tolower(const std::string& value);
    std::string trim(const std::string& value);
} // namespace Http::Ascii

namespace Http::Header {
    bool isValidName(const std::string& name);
    bool isValidValue(const std::string& value);
} // namespace Http::Header
namespace Http::TransferCoding {
    constexpr std::string_view Chunked = "chunked";
} // namespace Http::TransferCoding

namespace Http::ContentType {
    constexpr std::string_view Html = "text/html";
}

namespace Http::Server {
    constexpr std::string_view Name = "webserv";
} // namespace Http::Server
