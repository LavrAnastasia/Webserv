#pragma once

#include <string>
#include <string_view>

namespace Http::Syntax {
    constexpr char SP = ' ';
    constexpr char HTAB = '\t';
    constexpr char CR = '\r';
    constexpr char LF = '\n';

    constexpr std::string_view CrLf = "\r\n";
    constexpr std::string_view HeaderSectionEnd = "\r\n\r\n";

} // namespace Http::Syntax

namespace Http::Ascii {
    std::string tolower(const std::string& value);
    std::string trim(const std::string& value);
} // namespace Http::Ascii

namespace Http::Header {
    constexpr std::string_view ContentLength = "content-length";
    constexpr std::string_view TransferEncoding = "transfer-encoding";
    constexpr std::string_view ChunkedValue = "chunked";
    constexpr std::string_view Host = "host";
} // namespace Http::Header
