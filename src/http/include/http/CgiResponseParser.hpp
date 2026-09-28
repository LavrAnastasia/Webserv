#pragma once

#include <optional>
#include <string_view>

#include "http/HttpResponse.hpp"

class CgiResponseParser {
public:
    static std::optional<HttpResponse> parse(std::string_view output);
};
