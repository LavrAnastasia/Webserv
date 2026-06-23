#pragma once

#include "config/ConfigError.hpp"

class ConfigValidationError : public ConfigError {
public:
    enum class Reason {
        MissingDirective,
        DuplicateDirective,
        DuplicateValue,
        WrongArgumentCount,
        DirectiveNotAllowed,
        UnknownDirective,
        UnknownBlock,
        BlockNotAllowed,
        MissingBody,
        UnexpectedBody,
        ConflictingBehavior,
        EmptyConfig,
    };

    ConfigValidationError(Reason reason, const std::string& context = "");

private:
    static std::string formatMessage(Reason reason, const std::string& context);
};
