#pragma once

#include "config/ConfigError.hpp"

#include <filesystem>
#include <string>

class ConfigReadError : public ConfigError {
public:
    enum class Reason {
        FileDoesNotExist,
        NotRegularFile,
        FileTooLarge,
        OpenFailed,
        ReadFailed,
    };

    ConfigReadError(Reason reason, const std::filesystem::path& path);

private:
    static std::string formatMessage(Reason reason, const std::filesystem::path& path);
};
