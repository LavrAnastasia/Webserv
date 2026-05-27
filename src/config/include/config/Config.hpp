#pragma once

#include <vector>

#include "config/ServerConfig.hpp"

struct Config {
    std::vector<ServerConfig> servers;
};
