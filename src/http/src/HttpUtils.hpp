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
