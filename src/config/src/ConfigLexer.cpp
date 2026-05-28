#include "ConfigLexer.hpp"

#include <algorithm>
#include <cctype>
#include <optional>

namespace Config::Syntax {
constexpr char Comment = '#';
constexpr char LeftBrace = '{';
constexpr char RightBrace = '}';
constexpr char Semicolon = ';';
constexpr char Newline = '\n';
} // namespace Config::Syntax

namespace {
std::optional<TokenType> tokenTypeForSymbol(char c) {
    if (c == Config::Syntax::LeftBrace) {
        return TokenType::LeftBrace;
    }
    if (c == Config::Syntax::RightBrace) {
        return TokenType::RightBrace;
    }
    if (c == Config::Syntax::Semicolon) {
        return TokenType::Semicolon;
    }
    return std::nullopt;
}

bool isWordTerminator(unsigned char c) {
    return std::isspace(c) || c == Config::Syntax::LeftBrace || c == Config::Syntax::RightBrace ||
        c == Config::Syntax::Semicolon || c == Config::Syntax::Comment;
}
} // namespace

ConfigLexer::ConfigLexer(const std::string& source) : source_(source) {
}

std::vector<Token> ConfigLexer::run() {
    std::vector<Token> result{};

    for (auto it = source_.begin(); it != source_.end();) {
        const char current = *it;

        if (std::isspace(static_cast<unsigned char>(current))) {
            ++it;
            continue;
        }

        if (current == Config::Syntax::Comment) {
            it = std::find(it, source_.end(), Config::Syntax::Newline);
            continue;
        }

        if (const auto tokenType = tokenTypeForSymbol(current)) {
            result.push_back(Token{.type = *tokenType, .value = ""});
            ++it;
            continue;
        }

        const auto start = it;

        it = std::find_if(it, source_.end(), isWordTerminator);

        result.push_back(Token{
            .type = TokenType::Word,
            .value = std::string(start, it),
        });
    }


    return result;
}

std::vector<Token> ConfigLexer::tokenize(const std::string& source) {
    return ConfigLexer{source}.run();
}
