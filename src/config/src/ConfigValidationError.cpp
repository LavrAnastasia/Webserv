#include "ConfigValidationError.hpp"

ConfigValidationError::ConfigValidationError(Reason reason, const std::string& context)
    : ConfigError(formatMessage(reason, context)) {
}

std::string ConfigValidationError::formatMessage(Reason reason, const std::string& context) {
    switch (reason) {
        case Reason::MissingDirective:
            return "missing required directive: " + context;

        case Reason::DuplicateDirective:
            return "duplicate directive";

        case Reason::DuplicateValue:
            return "duplicate " + context;

        case Reason::WrongArgumentCount:
            return "wrong argument count for " + context;

        case Reason::DirectiveNotAllowed:
            return "directive '" + context + "' is not allowed in this block";

        case Reason::UnknownDirective:
            return "unknown directive '" + context + "'";

        case Reason::UnknownBlock:
            return "unknown block '" + context + "'";

        case Reason::BlockNotAllowed:
            return "block '" + context + "' is not allowed at this level";

        case Reason::MissingBody:
            return context + " must have a body";

        case Reason::UnexpectedBody:
            return context + " must not have a body";

        case Reason::ConflictingBehavior:
            return "location cannot define multiple handler behaviors";

        case Reason::EmptyConfig:
            return "configuration is empty";
    }

    return "unknown config validation error: " + context;
}
