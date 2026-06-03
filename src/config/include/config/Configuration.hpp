#pragma once

#include <vector>

#include "config/ServerConfig.hpp"

struct Configuration {
    std::vector<ServerConfig> servers;
};
