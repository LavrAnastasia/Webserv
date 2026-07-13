#pragma once

#include <string>
#include <string_view>

namespace Http::Ascii {
    std::string tolower(const std::string& value);
    std::string trim(const std::string& value);
} // namespace Http::Ascii

namespace Http::TransferCoding {
    constexpr std::string_view Chunked = "chunked";
} // namespace Http::TransferCoding
