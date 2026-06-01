#include "config/ConfigLoader.hpp"

#include <cstdint>
#include <fstream>
#include <sstream>

#include "ConfigLexer.hpp"
#include "ConfigReadError.hpp"

namespace {
    constexpr std::uintmax_t kMaxConfigFileSize = 1024 * 1024; // 1 MB
}

std::string ConfigLoader::read(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw ConfigReadError(ConfigReadError::Reason::FileDoesNotExist, path);
    }

    if (!std::filesystem::is_regular_file(path)) {
        throw ConfigReadError(ConfigReadError::Reason::NotRegularFile, path);
    }

    const auto fileSize = std::filesystem::file_size(path);

    if (fileSize > kMaxConfigFileSize) {
        throw ConfigReadError(ConfigReadError::Reason::FileTooLarge, path);
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        throw ConfigReadError(ConfigReadError::Reason::OpenFailed, path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    if (file.bad()) {
        throw ConfigReadError(ConfigReadError::Reason::ReadFailed, path);
    }

    return buffer.str();
}

Config ConfigLoader::load(const std::string& path) {
    const std::string source = ConfigLoader::read(std::filesystem::path{path});
    const std::vector<Token> tokens = ConfigLexer::tokenize(source);

    (void)tokens;
    // parse()
    // validate()
    // normalize()
    return Config{};
}
