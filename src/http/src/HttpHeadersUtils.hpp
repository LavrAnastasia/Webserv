#pragma once

#include <string>
#include <string_view>

namespace Http::Headers {
    constexpr std::string_view ContentLength = "Content-Length";
    constexpr std::string_view ContentType = "Content-Type";
    constexpr std::string_view TransferEncoding = "Transfer-Encoding";
    constexpr std::string_view Host = "Host";
    constexpr std::string_view Date = "Date";
    constexpr std::string_view Allow = "Allow";
    constexpr std::string_view Location = "Location";
    constexpr std::string_view Server = "Server";

    bool isValidName(const std::string& name);
    bool isValidValue(const std::string& value);
} // namespace Http::Headers
