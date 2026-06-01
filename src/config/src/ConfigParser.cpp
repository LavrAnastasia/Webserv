#include <optional>

#include "ConfigParser.hpp"
#include "ConfigSyntaxError.hpp"
#include "ConfigToken.hpp"

std::vector<ConfigNode> ConfigParser::buildBody(TokenIterator& it, TokenIterator end, const std::string& blockName) {
    std::vector<ConfigNode> nodes{};

    ++it;
    while (it != end && it->type != TokenType::RightBrace) {
        const auto node = buildNode(it, end);
        nodes.push_back(node);
    }
    if (it == end) {
        throw ConfigSyntaxError(ConfigSyntaxError::Reason::UnclosedBlock, blockName);
    }
    ++it;
    return nodes;
}

ConfigNode ConfigParser::buildNode(TokenIterator& it, TokenIterator end) {
    if (it->type == TokenType::RightBrace) {
        throw ConfigSyntaxError(ConfigSyntaxError::Reason::UnexpectedClosingBrace);
    }

    if (it->type != TokenType::Word) {
        throw ConfigSyntaxError(ConfigSyntaxError::Reason::ExpectedNodeName);
    }

    ConfigNode node{
        .name = it->value,
        .arguments = {},
        .body = std::nullopt,
    };

    ++it;

    while (it != end && it->type == TokenType::Word) {
        node.arguments.push_back(it->value);
        ++it;
    }

    if (it == end) {
        throw ConfigSyntaxError(ConfigSyntaxError::Reason::ExpectedTerminator, node.name);
    }

    if (it->type == TokenType::Semicolon) {
        ++it;
        return node;
    }

    if (it->type == TokenType::LeftBrace) {
        node.body = buildBody(it, end, node.name);
        return node;
    }

    throw ConfigSyntaxError(ConfigSyntaxError::Reason::ExpectedTerminator, node.name);
}

std::vector<ConfigNode> ConfigParser::parse(const std::vector<Token>& tokens) {
    std::vector<ConfigNode> result{};

    for (auto it = tokens.begin(); it != tokens.end();) {
        const auto node = buildNode(it, tokens.end());
        result.push_back(node);
    }

    return result;
}
