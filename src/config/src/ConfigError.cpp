#include "config/ConfigError.hpp"

ConfigError::ConfigError(const std::string& message) : std::runtime_error(message) {
}
