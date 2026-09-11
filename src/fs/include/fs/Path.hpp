#pragma once

#include <filesystem>

namespace Fs {
    bool isPrefixOf(const std::filesystem::path& base, const std::filesystem::path& path);

    std::filesystem::path resolve(const std::filesystem::path& root, const std::filesystem::path& path);

    bool hasDotComponents(const std::filesystem::path& path);
} // namespace Fs
