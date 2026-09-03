#pragma once

#include <filesystem>

namespace Fs {
    bool isPrefixOf(const std::filesystem::path& base, const std::filesystem::path& path);
} // namespace Fs
