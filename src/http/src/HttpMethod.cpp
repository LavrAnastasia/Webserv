#include "http/HttpMethod.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace {
    struct MethodEntry {
        HttpMethod method;
        std::string_view name;
    };

    constexpr std::array<MethodEntry, 3> kMethods = {{
        {HttpMethod::Get, "GET"},
        {HttpMethod::Post, "POST"},
        {HttpMethod::Delete, "DELETE"},
    }};
} // namespace

std::optional<HttpMethod> parseHttpMethod(const std::string& value) {
    auto it =
        std::find_if(kMethods.begin(), kMethods.end(), [&value](const MethodEntry& m) { return m.name == value; });

    return it == kMethods.end() ? std::nullopt : std::optional<HttpMethod>{it->method};
}

std::string toString(HttpMethod method) {
    auto it =
        std::find_if(kMethods.begin(), kMethods.end(), [method](const MethodEntry& m) { return m.method == method; });

    return it == kMethods.end() ? "" : std::string(it->name);
}
