#include <stdexcept>

#include "ConfigBuilder.hpp"
#include "ConfigValidator.hpp"

LocationConfig ConfigBuilder::buildLocationConfig(const ConfigNode& node) {
    ConfigValidator::validateBlock(Config::Block::Location, node);

    LocationConfig config{};

    config.path = node.arguments[0];

    for (const ConfigNode& child : *node.body) {
        ConfigValidator::validateDirective(Config::Block::Location, child);

        // write directive to Config
    }

    return config;
}

ServerConfig ConfigBuilder::buildServerConfig(const ConfigNode& node) {
    ConfigValidator::validateBlock(Config::Block::Server, node);

    ServerConfig config{};

    for (const ConfigNode& child : *node.body) {
        if (child.body.has_value()) {
            config.locations.push_back(buildLocationConfig(child));

        } else {
            ConfigValidator::validateDirective(Config::Block::Server, child);

            // write directive to Config
        }
    }

    return config;
}

Configuration ConfigBuilder::build(const std::vector<ConfigNode>& nodes) {
    if (nodes.size() == 0) {
        throw std::runtime_error("empty config");
    }

    Configuration config{};
    for (const auto& node : nodes) {
        config.servers.push_back(buildServerConfig(node));
    }

    return config;
}
