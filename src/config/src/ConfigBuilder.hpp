#pragma once

#include <vector>

#include "ConfigNode.hpp"
#include "config/Configuration.hpp"

class ConfigBuilder {
private:
    static ServerConfig buildServerConfig(const ConfigNode& node);
    static LocationConfig buildLocationConfig(const ConfigNode& node);

public:
    static Configuration build(const std::vector<ConfigNode>& nodes);
};
