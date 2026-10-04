#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace Files {

    inline void write(const std::filesystem::path& path, const std::string& body) {
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file;
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file.open(path, std::ios::binary);
        file.write(body.data(), static_cast<std::streamsize>(body.size()));
        file.close();
    }

    inline std::string read(const std::filesystem::path& path) {
        std::ifstream file;
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file.open(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

}
