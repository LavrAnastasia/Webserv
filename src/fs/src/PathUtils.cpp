#include "fs/PathUtils.hpp"

namespace Fs {
    bool contains(const std::filesystem::path& base, const std::filesystem::path& path) {
        auto pathIterator = path.begin();

        for (auto baseIterator = base.begin(); baseIterator != base.end(); ++baseIterator, ++pathIterator) {
            if (pathIterator == path.end() || *pathIterator != *baseIterator) {
                return false;
            }
        }

        return true;
    }
} // namespace Fs
