#include "fs/PathUtils.hpp"

#include <algorithm>

namespace Fs {
    bool isPrefixOf(const std::filesystem::path& base, const std::filesystem::path& path) {
        return std::ranges::mismatch(base, path).in1 == base.end();
    }
} // namespace Fs
