#pragma once

#include <string>

namespace Http::Url {
    std::string encodePath(const std::string& path);
    std::string encodeSegment(const std::string& value);
} // namespace Http::Url
