#include "ConfigLexer.hpp"

#include <algorithm>
#include <cctype>

namespace Config::Syntax {
constexpr char Comment = '#';
constexpr char LeftBrace = '{';
constexpr char RightBrace = '}';
constexpr char Semicolon = ';';
constexpr char Newline = '\n';
} // namespace Config::Syntax

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

        if (current == Config::Syntax::LeftBrace) {
            result.push_back(Token{.type = TokenType::LeftBrace, .value = ""});
            ++it;
            continue;
        }

        if (current == Config::Syntax::RightBrace) {
            result.push_back(Token{.type = TokenType::RightBrace, .value = ""});
            ++it;
            continue;
        }

        if (current == Config::Syntax::Semicolon) {
            result.push_back(Token{.type = TokenType::Semicolon, .value = ""});
            ++it;
            continue;
        }

        const auto start = it;

        it = std::find_if(it, source_.end(), [](unsigned char c) {
            return std::isspace(c) || c == Config::Syntax::LeftBrace || c == Config::Syntax::RightBrace ||
                c == Config::Syntax::Semicolon || c == Config::Syntax::Comment;
        });

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
