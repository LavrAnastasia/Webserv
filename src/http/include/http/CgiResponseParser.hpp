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
    struct Head {
        HttpResponse response;
        std::size_t consumed;
        std::optional<std::uintmax_t> length;
    };

    static std::size_t headerLimit();
    static std::variant<Incomplete, Invalid, Head> parseHeaders(std::string_view output);
    static std::optional<HttpResponse> parse(std::string_view output);
};
