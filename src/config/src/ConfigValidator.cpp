#include <string_view>

#include "ConfigValidator.hpp"


// validate Server Final Block

//  validate Location Final  Block

//  validate Final Config ? Noramlize?

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

// namespace Config::Directive {
//     constexpr std::string_view listen = "listen";
//     constexpr std::string_view root = "root";
//     constexpr std::string_view index = "index";

//     constexpr std::string_view clientMaxBodySize = "client_max_body_size";
//     constexpr std::string_view errorPage =  "error_page";
//     constexpr std::string_view methods = "methods";
//     constexpr std::string_view autoindex = "autoindex";
//     constexpr std::string_view upload_path = "upload_path";
//     constexpr std::string_view redirect = "return";
//     constexpr std::string_view cgi = "cgi";
// }


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
    if (!node.arguments.empty()) {
        throw std::runtime_error("Server block must not have arguments");
    }

    if (!node.body.has_value()) {
        throw std::runtime_error("Server block must have body");
    }
}

void ConfigValidator::validateLocationBlock(const ConfigNode& node) {
    if (node.arguments.size() != 1) {
        throw std::runtime_error("Location block has to have exactly one arg");
    }

    if (!node.body.has_value()) {
        throw std::runtime_error("Location block must have body");
    }
}

void ConfigValidator::validateDirective(Config::Block block, const ConfigNode& node) {
    switch (block) {
        case Config::Block::Server:
            break;
        case Config::Block::Location:
            if (node.body.has_value()) {
                throw std::runtime_error("Nested blocks are not allowed inside location block");
            }
            break;
    }
}
