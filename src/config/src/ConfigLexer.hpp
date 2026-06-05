#pragma once

#include <string>
#include <vector>

#include "ConfigToken.hpp"

class ConfigLexer {
private:
    explicit ConfigLexer(const std::string& source);

    std::vector<Token> run();

    const std::string& source_;

public:
    static std::vector<Token> tokenize(const std::string& source);
};
