#pragma once

#include "http/HttpMethod.hpp"
#include <optional>
#include <string>

namespace Http::Method {
    std::optional<HttpMethod> fromString(const std::string& value);
    std::string toString(HttpMethod method);
} // namespace Http::Method
