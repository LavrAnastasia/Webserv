#pragma once

#include <cstddef>
#include <vector>

#include "ConfigNode.hpp"
#include "ConfigToken.hpp"

class ConfigParser {
private:
    using TokenIterator = std::vector<Token>::const_iterator;

    static ConfigNode buildNode(TokenIterator& it, TokenIterator end, std::size_t depth);
    static std::vector<ConfigNode>
    buildBody(TokenIterator& it, TokenIterator end, const std::string& blockName, std::size_t depth);

public:
    static std::vector<ConfigNode> parse(const std::vector<Token>& tokens);
};
