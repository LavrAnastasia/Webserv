#pragma once

#include <filesystem>
#include <string>

#include "config/Configuration.hpp"

class ConfigLoader {
private:
    static std::string read(const std::filesystem::path& path);

public:
    static Configuration load(const std::string& path);
};
