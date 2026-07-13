#pragma once

#include <string>
#include <string_view>

namespace Http::Ascii {
    std::string tolower(const std::string& value);
    std::string trim(const std::string& value);
} // namespace Http::Ascii

namespace Http::Header {
    constexpr std::string_view ContentLength = "content-length";
    constexpr std::string_view TransferEncoding = "transfer-encoding";
    constexpr std::string_view Connection = "connection";
    constexpr std::string_view ChunkedValue = "chunked";
    constexpr std::string_view Host = "host";

    bool isValidName(const std::string& name);
    bool isValidValue(const std::string& value);
} // namespace Http::Header
