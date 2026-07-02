#pragma once

#include <string>

namespace Http::Syntax {
    constexpr char SP = ' ';
    constexpr char HTAB = '\t';
    constexpr char CR = '\r';
    constexpr char LF = '\n';
} // namespace Http::Syntax

namespace Http::Ascii {
    std::string tolower(const std::string& value);
    std::string trim(const std::string& value);
} // namespace Http::Ascii
