#pragma once

#include <filesystem>
#include <string>

#include "config/Config.hpp"

class ConfigLoader {
private:
    static std::string read(const std::filesystem::path& path);

    // tokenize
    // parse()
    // validate()
    // normalize()

public:
    static Config load(const std::string& path);
};
