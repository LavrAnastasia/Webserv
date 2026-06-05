#pragma once

#include "config/ConfigError.hpp"

#include <string>

class ConfigSyntaxError : public ConfigError {
public:
    enum class Reason { ExpectedNodeName, ExpectedTerminator, UnexpectedClosingBrace, UnclosedBlock };

    ConfigSyntaxError(Reason reason, const std::string& context = "");

private:
    static std::string formatMessage(Reason reason, const std::string& context);
};
