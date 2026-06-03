#pragma once

#include <vector>

#include "ConfigNode.hpp"
#include "config/Configuration.hpp"

class ConfigBuilder {
public:
    static Configuration build(const std::vector<ConfigNode>& nodes);
};
