#pragma once

#include "config/ConfigError.hpp"

class ConfigDecodingError : public ConfigError {
public:
    enum class Reason {
        InvalidFormat,
        EmptyValue,
        UnsupportedValue,
        OutOfRange,
        Duplicate,
    };

    ConfigDecodingError(Reason reason, const std::string& context);

private:
    static std::string formatMessage(Reason reason, const std::string& context);
};
