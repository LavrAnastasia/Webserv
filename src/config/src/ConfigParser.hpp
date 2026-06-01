#pragma once

#include <vector>

#include "ConfigNode.hpp"
#include "ConfigToken.hpp"

class ConfigParser {
public:
    std::vector<ConfigNode> parse(const std::vector<Token>& tokens);
};
