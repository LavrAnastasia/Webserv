#include <string_view>
#include <unordered_map>

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

    const std::unordered_map<std::string_view, Config::Directive> directives = {
        {"listen", Config::Directive::Listen},
        {"root", Config::Directive::Root},
        {"index", Config::Directive::Index},
        {"client_max_body_size", Config::Directive::ClientMaxBodySize},
        {"error_page", Config::Directive::ErrorPage},
        {"methods", Config::Directive::Methods},
        {"autoindex", Config::Directive::AutoIndex},
        {"upload_path", Config::Directive::UploadPath},
        {"return", Config::Directive::Return},
        {"cgi", Config::Directive::Cgi},
    };
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

Config::Directive ConfigValidator::validateDirective(Config::Block block, const ConfigNode& node) {
    const auto directive = directives.find(node.name);

    if (directive == directives.end()) {
        throw std::runtime_error("Unknown directive '" + node.name + "'");
    }

    switch (block) {
        case Config::Block::Server:
            break;
        case Config::Block::Location:
            if (node.body.has_value()) {
                throw std::runtime_error("Nested blocks are not allowed inside location block");
            }
            break;
    }

    return directive->second;
}
