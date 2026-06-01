#pragma once

#include <string>
#include <vector>

enum class TokenType { Word, LeftBrace, RightBrace, Semicolon };

struct Token {
    TokenType type;
    std::string value;
};

class ConfigLexer {
private:
    explicit ConfigLexer(const std::string& source);

    std::vector<Token> run();

    const std::string& source_;

public:
    static std::vector<Token> tokenize(const std::string& source);
};
