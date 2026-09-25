#include <algorithm>
#include <ranges>

#include "HttpSyntax.hpp"
#include "HttpUtils.hpp"
#include "http/HttpHeaders.hpp"

HttpHeaders::HttpHeaders(std::initializer_list<std::pair<std::string_view, std::string_view>> fields) {
    for (const auto& [name, value] : fields) {
        add(std::string(name), std::string(value));
    }
}

bool HttpHeaders::equals(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char lhs, char rhs) {
        return Http::Ascii::tolower(lhs) == Http::Ascii::tolower(rhs);
    });
}

void HttpHeaders::add(const std::string& name, const std::string& value) {
    _headers.emplace_back(name, value);
}

void HttpHeaders::set(const std::string& name, const std::string& value) {
    erase(name);
    add(name, value);
}

void HttpHeaders::erase(std::string_view name) {
    std::erase_if(_headers, [name](const auto& entry) { return equals(entry.first, name); });
}

bool HttpHeaders::has(std::string_view name) const {
    return std::ranges::any_of(_headers, [name](const auto& entry) { return equals(entry.first, name); });
}

bool HttpHeaders::has(std::string_view name, std::string_view token) const {
    const std::optional<std::string> value = get(name);

    if (!value) {
        return false;
    }

    return std::ranges::any_of(std::views::split(*value, Http::Syntax::ListSeparator), [token](const auto& part) {
        return equals(Http::Ascii::trim(std::string(part.begin(), part.end())), token);
    });
}

std::optional<std::string> HttpHeaders::get(std::string_view name) const {
    std::optional<std::string> result;

    for (const auto& [key, value] : _headers) {
        if (!equals(key, name)) {
            continue;
        }

        if (!result) {
            result = value;
        } else {
            result->append(1, Http::Syntax::ListSeparator).append(1, Http::Syntax::SP).append(value);
        }
    }

    return result;
}

std::string HttpHeaders::serialize() const {
    std::string output;

    for (const auto& [name, value] : _headers) {
        output.append(name)
            .append(1, Http::Syntax::HeaderKeyEnd)
            .append(1, Http::Syntax::SP)
            .append(value)
            .append(Http::Syntax::CRLF);
    }

    return output;
}
