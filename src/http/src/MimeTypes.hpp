#pragma once

#pragma once

#include <filesystem>
#include <string>

namespace Http::Mime {
    constexpr std::string_view Html = "text/html; charset=utf-8";

    std::string from(const std::filesystem::path& path);
} // namespace Http::Mime
