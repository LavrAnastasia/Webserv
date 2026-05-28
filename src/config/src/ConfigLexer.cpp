#include "ConfigLexer.hpp"

#include <cctype>

ConfigLexer::ConfigLexer(const std::string& source) : source_(source) {
    //
}

std::vector<Token> ConfigLexer::run() {
    std::vector<Token> result{};

    for (auto it = source_.begin(); it != source_.end();) {
        const char current = *it;

        if (std::isspace(static_cast<unsigned char>(current))) {
            ++it;
            continue;
        }

        if (current == '#') {
            it = std::find(it, source_.end(), '\n');
            continue;
        }

        if (current == '{') {
            result.push_back(Token{.type = TokenType::LeftBrace, .value = ""});
            ++it;
            continue;
        }

        if (current == '}') {
            result.push_back(Token{.type = TokenType::RightBrace, .value = ""});
            ++it;
            continue;
        }

        if (current == ';') {
            result.push_back(Token{.type = TokenType::Semicolon, .value = ""});
            ++it;
            continue;
        }

        const auto start = it;

        it = std::find_if(it, source_.end(), [](unsigned char c) {
            return std::isspace(c) || c == '{' || c == '}' || c == ';' || c == '#';
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
