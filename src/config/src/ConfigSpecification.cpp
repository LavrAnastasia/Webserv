#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "ConfigSpecification.hpp"

namespace {
    const std::unordered_map<std::string_view, Config::DirectiveRule> directiveRules = {
        {
            "listen",
            {.directive = Config::Directive::Listen, .contexts = {Config::Block::Server}, .repeatable = true},
        },
        {
            "root",
            {.directive = Config::Directive::Root, .contexts = {Config::Block::Server, Config::Block::Location}},
        },
        {
            "index",
            {.directive = Config::Directive::Index, .contexts = {Config::Block::Server, Config::Block::Location}},
        },
        {
            "client_max_body_size",
            {
                .directive = Config::Directive::ClientMaxBodySize,
                .contexts =
                    {
                        Config::Block::Server,
                        Config::Block::Location,
                    },
            },
        },
        {
            "error_page",
            {
                .directive = Config::Directive::ErrorPage,
                .contexts = {Config::Block::Server},
                .argumentCount =
                    {
                        2,
                        std::numeric_limits<std::size_t>::max(),
                    },
                .repeatable = true,
            },
        },
        {
            "methods",
            {
                .directive = Config::Directive::Methods,
                .contexts = {Config::Block::Location},
                .argumentCount = {1, std::numeric_limits<std::size_t>::max()},
            },
        },
        {
            "autoindex",
            {.directive = Config::Directive::AutoIndex, .contexts = {Config::Block::Location}},
        },
        {
            "upload_path",
            {.directive = Config::Directive::UploadPath, .contexts = {Config::Block::Location}},
        },
        {
            "return",
            {.directive = Config::Directive::Return, .contexts = {Config::Block::Location}, .argumentCount = {2, 2}},
        },
        {
            "cgi",
            {.directive = Config::Directive::Cgi, .contexts = {Config::Block::Location}, .argumentCount = {2, 2}},
        },
    };
}

const Config::DirectiveRule& ConfigSpecification::directiveRule(std::string_view name) {
    const auto ruleIt = directiveRules.find(name);

    if (ruleIt == directiveRules.end()) {
        throw std::runtime_error("Unknown directive '" + std::string(name) + "'");
    }

    return ruleIt->second;
}

const Config::DirectiveRule& ConfigSpecification::directiveRule(Config::Directive directive) {
    for (const auto& [name, rule] : directiveRules) {
        if (rule.directive == directive) {
            return rule;
        }
    }

    throw std::logic_error("Directive rule is missing");
}
