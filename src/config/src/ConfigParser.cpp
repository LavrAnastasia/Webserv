#include "ConfigParser.hpp"
#include "ConfigToken.hpp"

#include <stdexcept>

std::vector<ConfigNode> ConfigParser::buildBody(TokenIterator& it, TokenIterator end) {
    std::vector<ConfigNode> nodes{};

    ++it;
    while (it != end && it->type != TokenType::RightBrace) {
        const auto node = buildNode(it, end);
        nodes.push_back(node);
    }
    if (it == end) {
        throw std::runtime_error("Config ended unexpectedly");
    }
    ++it;
    return nodes;
}

ConfigNode ConfigParser::buildNode(TokenIterator& it, TokenIterator end) {
    if (it->type == TokenType::Word) {
        ConfigNode node{
            .name = it->value,
            .arguments = {},
            .body = std::nullopt,
        };

        ++it;

        while (it != end && it->type == TokenType::Word) {
            node.arguments.push_back(it->value);
            ++it;
        };

        if (it == end) {
            throw std::runtime_error("wrong config format");
        } else if (it->type == TokenType::Semicolon) {
            ++it;
            return node;
        } else if (it->type == TokenType::LeftBrace) {
            node.body = buildBody(it, end);
            return node;
        } else {
            throw std::runtime_error("wrong config format");
        }
    } else {
        throw std::runtime_error("wrong config format");
    }
};

std::vector<ConfigNode> ConfigParser::parse(const std::vector<Token>& tokens) {
    std::vector<ConfigNode> result{};

    for (auto it = tokens.begin(); it != tokens.end();) {
        const auto node = buildNode(it, tokens.end());
        result.push_back(node);
    }

    return result;
};
