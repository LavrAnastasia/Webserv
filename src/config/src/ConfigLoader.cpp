#include "config/ConfigLoader.hpp"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
    constexpr std::uintmax_t kMaxConfigFileSize = 1024 * 1024; // 1 MB
}

std::string ConfigLoader::read(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("config file does not exist: " + path.string());
    }

    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("config path is not a regular file: " + path.string());
    }

    const auto fileSize = std::filesystem::file_size(path);

    if (fileSize > kMaxConfigFileSize) {
        throw std::runtime_error("config file is too large: " + path.string());
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open config file: " + path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    if (file.bad()) {
        throw std::runtime_error("failed to read config file: " + path.string());
    }

    return buffer.str();
}

Config ConfigLoader::load(const std::string& path) {
    const std::string source = ConfigLoader::read(std::filesystem::path{path});

    (void)source;
    return Config{};
}
