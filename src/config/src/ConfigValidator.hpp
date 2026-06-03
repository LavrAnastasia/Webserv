#pragma once

#include "ConfigBlock.hpp"
#include "ConfigDirective.hpp"
#include "ConfigNode.hpp"

class ConfigValidator {
private:
    static void validateServerBlock(const ConfigNode& node);
    static void validateLocationBlock(const ConfigNode& node);

public:
    static void validateBlock(Config::Block block, const ConfigNode& node);
    static Config::Directive validateDirective(Config::Block block, const ConfigNode& node);
};
