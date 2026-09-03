#pragma once

#pragma once

#include <filesystem>
#include <string>

namespace Http::Mime {
    std::string from(const std::filesystem::path& path);
} // namespace Http::Mime
