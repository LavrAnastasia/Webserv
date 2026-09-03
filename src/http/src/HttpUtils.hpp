#pragma once

#include <string>

namespace Http::Ascii {
    char tolower(char c);
    std::string tolower(const std::string& value);
    std::string trim(const std::string& value);
} // namespace Http::Ascii
