#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "ConfigValidator.hpp"


// validate Server Final Block

//  validate Location Final  Block

//  validate Final Config ? Noramlize?

namespace {
    const std::unordered_map<std::string_view, Config::Block> blocks = {
        {"server", Config::Block::Server},
        {"location", Config::Block::Location},
    };

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

    const std::unordered_set<Config::Directive> serverDirectives = {
        Config::Directive::Listen,
        Config::Directive::Root,
        Config::Directive::Index,
        Config::Directive::ClientMaxBodySize,
        Config::Directive::ErrorPage,
    };

    const std::unordered_set<Config::Directive> locationDirectives = {
        Config::Directive::Root,
        Config::Directive::Index,
        Config::Directive::ClientMaxBodySize,
        Config::Directive::Methods,
        Config::Directive::AutoIndex,
        Config::Directive::UploadPath,
        Config::Directive::Return,
        Config::Directive::Cgi,
    };
} // namespace


void ConfigValidator::validateBlock(Config::Block block, const ConfigNode& node) {
    const auto blockIt = blocks.find(node.name);
    ;

    if (blockIt == blocks.end()) {
        throw std::runtime_error("Unknown block '" + node.name + "'");
    }

    if (block != blockIt->second) {
        throw std::runtime_error("Block '" + node.name + "' is not allowed at this level");
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
    const auto directiveIt = directives.find(node.name);

    if (directiveIt == directives.end()) {
        throw std::runtime_error("Unknown directive '" + node.name + "'");
    }

    if (node.body.has_value()) {
        throw std::runtime_error("Directive '" + node.name + "' must not have body");
    }

    const auto directive = directiveIt->second;
    switch (block) {
        case Config::Block::Server:
            if (!serverDirectives.contains(directive)) {
                throw std::runtime_error("Directive " + node.name + "is not allowed in server block");
            }
            break;
        case Config::Block::Location:
            if (!locationDirectives.contains(directive)) {
                throw std::runtime_error("Directive " + node.name + "is not allowed in server block");
            }
            break;
    }

    return directiveIt->second;
}
