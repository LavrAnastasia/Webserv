#pragma once

#include <cstddef>
#include <string_view>
#include <unordered_set>

#include "ConfigBlock.hpp"
#include "ConfigDirective.hpp"

namespace Config {
    struct ArgumentCount {
        std::size_t min;
        std::size_t max;
    };

    struct DirectiveRule {
        Directive directive;
        std::unordered_set<Block> contexts;
        ArgumentCount argumentCount = {1, 1};
        bool repeatable = false;
    };
} // namespace Config

class ConfigSpecification {
public:
    static const Config::DirectiveRule& directiveRule(std::string_view name);
    static const Config::DirectiveRule& directiveRule(Config::Directive directive);
};
