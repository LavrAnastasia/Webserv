#include "ConfigLexer.hpp"

ConfigLexer::ConfigLexer(const std::string& source) : source_(source) {
}

std::vector<Token> ConfigLexer::run() {
    return std::vector<Token>{Token{.type = TokenType::Word, .value = "smth"}};
}

std::vector<Token> ConfigLexer::tokenize(const std::string& source) {
    return ConfigLexer{source}.run();
}
