#include <string_view>

#include "ConfigValidator.hpp"

// validate Server Node

// validate Server Directive

// validate Server Block

// validate Location Node

// validate Location Directive

//  validate Location Block

//  validate Config ? Noramlize?

namespace {
    constexpr std::string_view blockName(Config::Block block) {
        switch (block) {
            case Config::Block::Server:
                return "server";

            case Config::Block::Location:
                return "location";
        }

        throw std::logic_error("Unknown config block type");
    }
} // namespace


void ConfigValidator::validateBlock(Config::Block block, const ConfigNode& node) {
    const std::string_view expectedName = blockName(block);

    if (node.name != expectedName) {
        throw std::runtime_error("Expected '" + std::string(expectedName) + "' block, got '" + node.name + "'");
    }
    switch (block) {
        case Config::Block::Server:
            validateServerBlock(node);
            break;
        case Config::Block::Location:
            validateLocationBlock(node);
            break;
    }
}

void ConfigValidator::validateServerBlock(const ConfigNode& node) {
    (void)node;

    // other checks
}

void ConfigValidator::validateLocationBlock(const ConfigNode& node) {
    (void)node;

    // other checks
}

void ConfigValidator::validateDirective(Config::Block type, const ConfigNode& node) {
    (void)node;
    switch (type) {
        case Config::Block::Server:
            break;
        case Config::Block::Location:
            break;
    }
}
