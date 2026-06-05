#pragma once

#include <string>

enum class TokenType { Word, LeftBrace, RightBrace, Semicolon };

struct Token {
    TokenType type;
    std::string value;
};
