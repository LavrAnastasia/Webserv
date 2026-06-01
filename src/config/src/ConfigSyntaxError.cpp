#include "ConfigSyntaxError.hpp"

ConfigSyntaxError::ConfigSyntaxError(Reason reason, const std::string& context)
    : ConfigError(formatMessage(reason, context)) {
}

std::string ConfigSyntaxError::formatMessage(Reason reason, const std::string& context) {
    switch (reason) {
        case Reason::ExpectedNodeName:
            return "expected directive or block name";

        case Reason::ExpectedTerminator:
            return "expected ';' or '{' after '" + context + "'";

        case Reason::UnexpectedClosingBrace:
            return "unexpected '}' at top level";

        case Reason::UnclosedBlock:
            return "expected '}' to close block '" + context + "'";
    }

    return "unknown config syntax error";
}
