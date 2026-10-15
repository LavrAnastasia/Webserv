#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

#include "http/HttpResponse.hpp"

class CgiResponseParser {
public:
    struct Incomplete {};
    struct Invalid {};
    struct Headers {
        HttpResponse response;
        std::size_t consumed;
        std::optional<std::uintmax_t> length;
    };

    static std::variant<Incomplete, Invalid, Headers> parseHeaders(std::string_view output);
};
