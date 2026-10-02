#pragma once

#include <string>

namespace Http::Ascii {
    bool isdigit(char c);
    bool isxdigit(char c);
    bool isalnum(char c);
    char tolower(char c);
    std::string tolower(const std::string& value);
    std::string trim(const std::string& value);
} // namespace Http::Ascii
