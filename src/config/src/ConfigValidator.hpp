#pragma once

#include <filesystem>
#include <unordered_map>
#include <unordered_set>

#include "config/Configuration.hpp"

#include "ConfigBlock.hpp"
#include "ConfigDirective.hpp"
#include "ConfigNode.hpp"


class ConfigValidator {
private:
    static void validateServerBlock(const ConfigNode& node);
    static void validateLocationBlock(const ConfigNode& node);

public:
    static void validate(const Configuration& config);
    static void validate(const LocationConfig& config);
    static void validate(const ServerConfig& config);
    static void validateBlock(Config::Block block, const ConfigNode& node);
    static Config::Directive validateDirective(Config::Block block, const ConfigNode& node);
    static void
    validateDirectiveDuplication(const std::unordered_set<Config::Directive>& directives, Config::Directive directive);
    static void validateErrorPages(
        const std::unordered_map<int, std::filesystem::path>& oldPages,
        const std::unordered_map<int, std::filesystem::path>& newPages
    );
};
