#pragma once

#include <string>

#include "config/Config.hpp"

class ConfigLoader {
    std::string read() const;

    // tokenize
    // parse()
    // validate()
    // normalize()

public:
    static Config load(const std::string& path);
};
