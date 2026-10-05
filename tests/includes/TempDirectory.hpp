#pragma once

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

class TempDirectory {
public:
    explicit TempDirectory(const std::string& prefix) {
        std::string pattern = (std::filesystem::temp_directory_path() / (prefix + "-XXXXXX")).string();
        char* directory = ::mkdtemp(pattern.data());
        if (directory == nullptr) {
            throw std::system_error(errno, std::generic_category(), "mkdtemp");
        }
        path_ = directory;
    }

    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        EXPECT_FALSE(error) << "Cannot remove test directory " << path_ << ": " << error.message();
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};
