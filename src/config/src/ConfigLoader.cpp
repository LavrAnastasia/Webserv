#include "config/ConfigLoader.hpp"

std::string ConfigLoader::read() const {
    return "";
}

Config ConfigLoader::load(const std::string& path) {
    (void)path;
    return Config{};
}
