#pragma once

#include <optional>
#include <string>
#include <vector>

struct ConfigNode {
    std::string name;
    std::vector<std::string> arguments;
    std::optional<std::vector<ConfigNode>> body;
};
