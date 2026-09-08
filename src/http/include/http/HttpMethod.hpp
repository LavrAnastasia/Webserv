#pragma once

#include <optional>
#include <set>
#include <string>

enum class HttpMethod { Get, Post, Delete };

namespace Http::Method {
    std::optional<HttpMethod> fromString(const std::string& value);
    std::string toString(HttpMethod method);
    std::string toString(const std::set<HttpMethod>& methods);
} // namespace Http::Method
