#include <algorithm>
#include <ranges>


#include "HttpUtils.hpp"
#include "http/HttpHeaders.hpp"

bool HttpHeaders::equals(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char lhs, char rhs) {
        return Http::Ascii::tolower(lhs) == Http::Ascii::tolower(rhs);
    });
}

std::map<std::string, std::string>::const_iterator HttpHeaders::find(std::string_view name) const {
    return std::ranges::find_if(_headers, [name](const auto& entry) { return equals(entry.first, name); });
}

bool HttpHeaders::set(const std::string& name, const std::string& value) {
    if (has(name)) {
        return false;
    }

    return _headers.emplace(name, value).second;
}

bool HttpHeaders::has(std::string_view name) const {
    return find(name) != _headers.end();
}

bool HttpHeaders::has(std::string_view name, std::string_view token) const {
    const auto it = find(name);

    if (it == _headers.end()) {
        return false;
    }

    return std::ranges::any_of(std::views::split(it->second, ','), [token](const auto& part) {
        return equals(Http::Ascii::trim(std::string(part.begin(), part.end())), token);
    });
}

std::optional<std::string> HttpHeaders::get(const std::string& name) const {
    const auto it = find(name);

    if (it == _headers.end()) {
        return std::nullopt;
    }
    return it->second;
}
