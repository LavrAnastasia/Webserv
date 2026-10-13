#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "http/HttpSerializer.hpp"

class CgiStream {
public:
    explicit CgiStream(HttpSerializer::Framing framing);

    bool receive(std::string_view output, std::string& bytes);
    bool end(std::string& bytes);
    bool hasHeaders() const;

private:
    bool append(std::string_view output, std::string& bytes);

    HttpSerializer::Framing framing_;
    std::string headers_;
    std::optional<std::uintmax_t> length_;
    std::uintmax_t received_ = 0;
    bool parsed_ = false;
    bool suppressed_ = false;
    bool ended_ = false;
};
