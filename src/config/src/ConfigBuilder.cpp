#include <stdexcept>
#include <unordered_set>

#include "ConfigBuilder.hpp"
#include "ConfigDecoder.hpp"
#include "ConfigDirective.hpp"
#include "ConfigValidator.hpp"

LocationConfig ConfigBuilder::buildLocationConfig(const ConfigNode& node) {
    ConfigValidator::validateBlock(Config::Block::Location, node);

    LocationConfig config{};

    config.path = ConfigDecoder::decodeLocationPath(node.arguments[0]);

    std::unordered_set<Config::Directive> directives;

    for (const ConfigNode& child : *node.body) {
        const auto directive = ConfigValidator::validateDirective(Config::Block::Location, child);

        ConfigValidator::validateDirectiveDuplication(directives, directive);
        directives.insert(directive);

        switch (directive) {
            case Config::Directive::Root:
                config.root = ConfigDecoder::decodeRoot(child.arguments[0]);
                break;
            case Config::Directive::Index:
                config.index = ConfigDecoder::decodeIndex(child.arguments[0]);
                break;
            case Config::Directive::ClientMaxBodySize:
                config.clientMaxBodySize = ConfigDecoder::decodeClientMaxBodySize(child.arguments);
                break;
            case Config::Directive::AutoIndex:
                config.autoindex = ConfigDecoder::decodeAutoIndex(child.arguments[0]);
                break;
            case Config::Directive::Methods:
                config.allowedMethods = ConfigDecoder::decodeMethods(child.arguments);
                break;
            case Config::Directive::UploadPath:
                config.upload = ConfigDecoder::decodeUpload(child.arguments[0]);
                break;
            case Config::Directive::Return:
                config.redirect = ConfigDecoder::decodeRedirect(child.arguments[0], child.arguments[1]);
                break;
            case Config::Directive::Cgi:
                config.cgi = ConfigDecoder::decodeCgi(child.arguments[0], child.arguments[1]);
                break;
            default:
                break;
        }
    }

    return config;
}

ServerConfig ConfigBuilder::buildServerConfig(const ConfigNode& node) {
    ConfigValidator::validateBlock(Config::Block::Server, node);

    ServerConfig config{};

    std::unordered_set<Config::Directive> directives;

    for (const ConfigNode& child : *node.body) {
        if (child.body.has_value()) {
            config.locations.push_back(buildLocationConfig(child));
        } else {
            const auto directive = ConfigValidator::validateDirective(Config::Block::Server, child);

            ConfigValidator::validateDirectiveDuplication(directives, directive);
            directives.insert(directive);

            switch (directive) {
                case Config::Directive::Listen:
                    config.listen.push_back(ConfigDecoder::decodeListen(child.arguments[0]));
                    break;
                case Config::Directive::Root:
                    config.root = ConfigDecoder::decodeRoot(child.arguments[0]);
                    break;
                case Config::Directive::Index:
                    config.index = ConfigDecoder::decodeIndex(child.arguments[0]);
                    break;
                case Config::Directive::ClientMaxBodySize:
                    config.clientMaxBodySize = ConfigDecoder::decodeClientMaxBodySize(child.arguments);
                    break;
                case Config::Directive::ErrorPage: {
                    const auto pages = ConfigDecoder::decodeErrorPage(child.arguments);

                    ConfigValidator::validateErrorPages(config.errorPages, pages);

                    config.errorPages.insert(pages.begin(), pages.end());
                    break;
                }
                default:
                    break;
            }
        }
    }

    return config;
}

Configuration ConfigBuilder::build(const std::vector<ConfigNode>& nodes) {
    if (nodes.empty()) {
        throw std::runtime_error("empty config");
    }

    Configuration config{};
    for (const auto& node : nodes) {
        config.servers.push_back(buildServerConfig(node));
    }

    return config;
}
