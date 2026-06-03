#pragma once

#include "ConfigBlock.hpp"
#include "ConfigDirective.hpp"
#include "ConfigNode.hpp"
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

class ConfigValidator {
private:
    static void validateServerBlock(const ConfigNode& node);
    static void validateLocationBlock(const ConfigNode& node);

public:
    static void validateBlock(Config::Block block, const ConfigNode& node);
    static Config::Directive validateDirective(Config::Block block, const ConfigNode& node);
    static void
    validateDirectiveDuplication(const std::unordered_set<Config::Directive>& directives, Config::Directive directive);
    static void validateErrorPages(
        const std::unordered_map<int, std::filesystem::path>& oldPages,
        const std::unordered_map<int, std::filesystem::path>& newPages
    );
};
