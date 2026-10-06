#include "log/Log.hpp"

#include <cstddef>
#include <ctime>
#include <iostream>
#include <string>

namespace {
    constexpr std::size_t kTimestampSize = sizeof("YYYY/MM/DD HH:MM:SS");

    std::string timestamp() {
        const std::time_t now = std::time(nullptr);
        const std::tm* local = std::localtime(&now);
        char buffer[kTimestampSize]{};

        if (local == nullptr || std::strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M:%S", local) == 0) {
            return "-";
        }

        return buffer;
    }

    void write(std::string_view level, std::string_view message) {
        try {
            std::string line = timestamp();

            line.append(" [").append(level).append("] ").append(message).append("\n");
            std::cerr << line;
        } catch (...) {
        }
    }
} // namespace

void Log::info(std::string_view message) {
    write("info", message);
}

void Log::warn(std::string_view message) {
    write("warn", message);
}

void Log::error(std::string_view message) {
    write("error", message);
}
