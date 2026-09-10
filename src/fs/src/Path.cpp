#include "fs/Path.hpp"

#include <algorithm>

namespace Fs {
    bool isPrefixOf(const std::filesystem::path& base, const std::filesystem::path& path) {
        return std::ranges::mismatch(base, path).in1 == base.end();
    }

    std::filesystem::path resolve(const std::filesystem::path& root, const std::filesystem::path& path) {
        return root / path.relative_path();
    }

    bool hasDotComponents(const std::filesystem::path& path) {
        return std::ranges::any_of(path, [](const std::filesystem::path& component) {
            return component == "." || component == "..";
        });
    }
} // namespace Fs
