#include <string>
#include <string_view>

#include "ConfigSpecification.hpp"
#include "ConfigValidationError.hpp"
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
        throw ConfigValidationError(ConfigValidationError::Reason::WrongArgumentCount, "server block");
    }

    if (!node.body.has_value()) {
        throw ConfigValidationError(ConfigValidationError::Reason::MissingBody, "server block");
    }
}

void ConfigValidator::validateLocationBlock(const ConfigNode& node) {
    if (node.arguments.size() != 1) {
        throw ConfigValidationError(ConfigValidationError::Reason::WrongArgumentCount, "location block");
    }

    if (!node.body.has_value()) {
        throw ConfigValidationError(ConfigValidationError::Reason::MissingBody, "location block");
    }
}

// Public

void ConfigValidator::validate(const Configuration& config) {
    std::unordered_set<std::string> endpoints;

    for (const ServerConfig& server : config.servers) {
        for (const ListenConfig& listen : server.listen) {
            const std::string endpoint = listen.host + ":" + std::to_string(listen.port);

            if (!endpoints.insert(endpoint).second) {
                throw ConfigValidationError(
                    ConfigValidationError::Reason::DuplicateValue, "listen endpoint: " + endpoint
                );
            }
        }
    }
}

void ConfigValidator::validate(const LocationConfig& config) {
    const int behaviorCount = static_cast<int>(config.redirect.has_value()) +
        static_cast<int>(config.upload.has_value()) + static_cast<int>(!config.cgi.empty());

    if (behaviorCount > 1) {
        throw ConfigValidationError(ConfigValidationError::Reason::ConflictingBehavior);
    }

    if (config.allowedMethods.empty()) {
        throw ConfigValidationError(ConfigValidationError::Reason::MissingDirective, "method");
    }
}
void ConfigValidator::validate(const ServerConfig& config) {
    if (config.listen.empty()) {
        throw ConfigValidationError(ConfigValidationError::Reason::MissingDirective, "listen");
    }

    if (config.root.empty()) {
        throw ConfigValidationError(ConfigValidationError::Reason::MissingDirective, "root");
    }

    if (config.locations.empty()) {
        throw ConfigValidationError(ConfigValidationError::Reason::MissingDirective, "location");
    }

    std::unordered_set<std::string> locationPaths;

    for (const LocationConfig& location : config.locations) {
        if (!locationPaths.insert(location.path).second) {
            throw ConfigValidationError(
                ConfigValidationError::Reason::DuplicateValue, "location path: " + location.path
            );
        }
    }
}

void ConfigValidator::validateBlock(Config::Block block, const ConfigNode& node) {
    const auto blockIt = blocks.find(node.name);

    if (blockIt == blocks.end()) {
        throw ConfigValidationError(ConfigValidationError::Reason::UnknownBlock, node.name);
    }

    if (block != blockIt->second) {
        throw ConfigValidationError(ConfigValidationError::Reason::BlockNotAllowed, node.name);
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
        throw ConfigValidationError(ConfigValidationError::Reason::UnexpectedBody, node.name);
    }

    if (!rule.contexts.contains(block)) {
        throw ConfigValidationError(ConfigValidationError::Reason::DirectiveNotAllowed, node.name);
    }

    if (node.arguments.size() < rule.argumentCount.min || node.arguments.size() > rule.argumentCount.max) {
        throw ConfigValidationError(ConfigValidationError::Reason::WrongArgumentCount, node.name);
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
        throw ConfigValidationError(ConfigValidationError::Reason::DuplicateDirective);
    }
}

void ConfigValidator::validateErrorPages(
    const std::unordered_map<int, std::filesystem::path>& oldPages,
    const std::unordered_map<int, std::filesystem::path>& newPages
) {
    for (const auto& page : newPages) {
        const int statusCode = page.first;

        if (oldPages.contains(statusCode)) {
            throw ConfigValidationError(ConfigValidationError::Reason::DuplicateValue, "error page status code");
        }
    }
}

void ConfigValidator::validateCgiDuplication(
    const std::unordered_map<std::string, CgiConfig>& cgis, const std::string& extension
) {
    if (cgis.contains(extension)) {
        throw ConfigValidationError(ConfigValidationError::Reason::DuplicateValue, "CGI extension: " + extension);
    }
}
