#include <stdexcept>

#include "ConfigBuilder.hpp"
#include "ConfigValidator.hpp"

Configuration ConfigBuilder::build(const std::vector<ConfigNode>& nodes) {
    if (nodes.size() == 0) {
        throw std::runtime_error("empty config");
    }

    for (const auto& node : nodes) {
        ConfigValidator::validateBlock(Config::Block::Server, node);
    }

    return {};
}
