#pragma once

#include "fs/FileDescriptor.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <variant>

class ResponseBody {
public:
    ResponseBody();
    explicit ResponseBody(std::string data);

    static std::variant<ResponseBody, std::error_code> open(const std::filesystem::path& path);

    std::uintmax_t size() const;
    bool done() const;
    bool isInMemory() const;
    // Replaces out with the next chunk; clears it on completion or read failure.
    bool next(std::string& out, std::size_t limit);

private:
    ResponseBody(FileDescriptor file, std::uintmax_t size);

    std::variant<std::string, FileDescriptor> source_;
    std::uintmax_t size_ = 0;
    std::uintmax_t offset_ = 0;
};
