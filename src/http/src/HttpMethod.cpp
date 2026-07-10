#include "HttpMethodUtils.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace Http::Method {
    namespace {
        struct Entry {
            HttpMethod method;
            std::string_view name;
        };

        constexpr std::array<Entry, 3> kMethods = {{
            {HttpMethod::Get, "GET"},
            {HttpMethod::Post, "POST"},
            {HttpMethod::Delete, "DELETE"},
        }};
    } // namespace

    std::optional<HttpMethod> fromString(const std::string& value) {
        auto it = std::find_if(kMethods.begin(), kMethods.end(), [&value](const Entry& m) { return m.name == value; });

        return it == kMethods.end() ? std::nullopt : std::optional<HttpMethod>{it->method};
    }

    std::string toString(HttpMethod method) {
        auto it =
            std::find_if(kMethods.begin(), kMethods.end(), [method](const Entry& m) { return m.method == method; });

        if (it == kMethods.end())
            throw std::logic_error("toString: unhandled HttpMethod");
        return std::string(it->name);
    }

} // namespace Http::Method
