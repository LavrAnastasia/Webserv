#include <stdexcept>
#include <string_view>

#include "ConfigSpecification.hpp"
#include "ConfigValidator.hpp"

namespace {
    const std::unordered_map<std::string_view, Config::Block> blocks = {
        {"server", Config::Block::Server},
        {"location", Config::Block::Location},
    };
} // namespace

// Private

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

// Public

void ConfigValidator::validate(const Configuration& config) {
    std::unordered_set<std::string> endpoints;

    for (const ServerConfig& server : config.servers) {
        for (const ListenConfig& listen : server.listen) {
            const std::string endpoint = listen.host + ":" + std::to_string(listen.port);

            if (!endpoints.insert(endpoint).second) {
                throw std::runtime_error("Duplicate listen endpoint: " + endpoint);
            }
        }
    }
}

void ConfigValidator::validate(const LocationConfig& config) {
    const int behaviorCount = static_cast<int>(config.redirect.has_value()) +
        static_cast<int>(config.upload.has_value()) + static_cast<int>(config.cgi.has_value());

    if (behaviorCount > 1) {
        throw std::runtime_error("Location cannot define multiple handler behaviors");
    }

    if (config.allowedMethods.empty()) {
        throw std::runtime_error("Server must define at least one method directive");
    }
}
void ConfigValidator::validate(const ServerConfig& config) {
    if (config.listen.empty()) {
        throw std::runtime_error("Server must define at least one listen directive");
    }

    if (config.root.empty()) {
        throw std::runtime_error("Server must define at least one root directive");
    }

    if (config.locations.empty()) {
        throw std::runtime_error("Server must define at least one location");
    }

    std::unordered_set<std::string> locationPaths;

    for (const LocationConfig& location : config.locations) {
        if (!locationPaths.insert(location.path).second) {
            throw std::runtime_error("Duplicate location path: " + location.path);
        }
    }
}

void ConfigValidator::validateBlock(Config::Block block, const ConfigNode& node) {
    const auto blockIt = blocks.find(node.name);

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

Config::Directive ConfigValidator::validateDirective(Config::Block block, const ConfigNode& node) {
    const auto& rule = ConfigSpecification::directiveRule(node.name);

    if (node.body.has_value()) {
        throw std::runtime_error("Directive '" + node.name + "' must not have body");
    }

    if (!rule.contexts.contains(block)) {
        throw std::runtime_error("Directive " + node.name + "is not allowed in this block");
    }

    if (node.arguments.size() < rule.argumentCount.min || node.arguments.size() > rule.argumentCount.max) {
        throw std::runtime_error("Wrong argument count for directive '" + node.name + "'");
    }

    return rule.directive;
}

void ConfigValidator::validateDirectiveDuplication(
    const std::unordered_set<Config::Directive>& directives, Config::Directive directive
) {
    const auto& rule = ConfigSpecification::directiveRule(directive);

    if (rule.repeatable) {
        return;
    }

    if (directives.contains(directive)) {
        throw std::runtime_error("Directive is duplicated");
    }
}

void ConfigValidator::validateErrorPages(
    const std::unordered_map<int, std::filesystem::path>& oldPages,
    const std::unordered_map<int, std::filesystem::path>& newPages
) {
    for (const auto& page : newPages) {
        const int statusCode = page.first;

        if (oldPages.contains(statusCode)) {
            throw std::runtime_error("Duplicate error_page status code");
        }
    }
}
