#pragma once

#include "http/HttpMethod.hpp"
#include <optional>
#include <set>
#include <string>

namespace Http::Method {
    std::optional<HttpMethod> fromString(const std::string& value);
    std::string toString(HttpMethod method);
    std::string toString(const std::set<HttpMethod>& methods);
} // namespace Http::Method
