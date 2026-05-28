#include "ConfigLexer.hpp"

#include <cctype>

ConfigLexer::ConfigLexer(const std::string& source) : source_(source) {
    //
}

std::vector<Token> ConfigLexer::run() {
    std::vector<Token> result{};
    const std::size_t size = source_.size();
    for (std::size_t i = 0; i < size; ++i) {
        if (std::isspace(static_cast<unsigned char>(source_[i]))) {
            continue;
        }

        // skip comments
        if (source_[i] == '#') {
            auto pos = source_.find('\n', i);
            if (pos == std::string::npos) {
                break;
            }
            i = pos;
            continue;
        }

        if (source_[i] == '{') {
            result.push_back(Token{.type = TokenType::LeftBrace, .value = ""});
            continue;
        }

        if (source_[i] == '}') {
            result.push_back(Token{.type = TokenType::RightBrace, .value = ""});
            continue;
        }

        if (source_[i] == ';') {
            result.push_back(Token{.type = TokenType::Semicolon, .value = ""});
            continue;
        }

        const std::size_t start = i;
        const auto end = source_.find_first_of(" \t\r\n{};#", i);

        if (end == std::string::npos) {
            result.push_back(Token{.type = TokenType::Word, .value = source_.substr(start)});
            break;
        }

        result.push_back(Token{.type = TokenType::Word, .value = source_.substr(start, end - start)});
        i = end - 1;
    }
    return result;
}

std::vector<Token> ConfigLexer::tokenize(const std::string& source) {
    return ConfigLexer{source}.run();
}
