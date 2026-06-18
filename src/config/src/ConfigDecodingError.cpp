#include "ConfigDecodingError.hpp"

ConfigDecodingError::ConfigDecodingError(Reason reason, const std::string& context)
    : ConfigError(formatMessage(reason, context)) {
}

std::string ConfigDecodingError::formatMessage(Reason reason, const std::string& context) {
    switch (reason) {
        case Reason::InvalidFormat:
            return "invalid format: " + context;

        case Reason::EmptyValue:
            return context + " must not be empty";

        case Reason::UnsupportedValue:
            return "unsupported value: " + context;

        case Reason::OutOfRange:
            return context + " is out of range";

        case Reason::Duplicate:
            return "duplicate " + context;
    }

    return "unknown config decoding error: " + context;
}
