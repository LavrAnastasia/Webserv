#include "ConfigReadError.hpp"

ConfigReadError::ConfigReadError(Reason reason, const std::filesystem::path& path)
    : ConfigError(formatMessage(reason, path)) {
}

std::string ConfigReadError::formatMessage(Reason reason, const std::filesystem::path& path) {
    const std::string pathString = path.string();

    switch (reason) {
        case Reason::FileDoesNotExist:
            return "config file does not exist: " + pathString;

        case Reason::NotRegularFile:
            return "config path is not a regular file: " + pathString;

        case Reason::FileTooLarge:
            return "config file is too large: " + pathString;

        case Reason::OpenFailed:
            return "failed to open config file: " + pathString;

        case Reason::ReadFailed:
            return "failed to read config file: " + pathString;
    }

    return "unknown config read error: " + pathString;
}
