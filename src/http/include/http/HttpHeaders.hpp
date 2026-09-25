#pragma once

#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

class HttpHeaders {
private:
    std::map<std::string, std::string> _headers;

    std::map<std::string, std::string>::const_iterator find(std::string_view name) const;

public:
    HttpHeaders() = default;
    explicit HttpHeaders(std::initializer_list<std::pair<std::string_view, std::string_view>> fields);

    static bool equals(std::string_view a, std::string_view b);

    bool set(const std::string& name, const std::string& value);
    bool has(std::string_view name) const;
    bool has(std::string_view name, std::string_view token) const;
    std::optional<std::string> get(const std::string& name) const;
    auto begin() const noexcept { return _headers.begin(); }

    auto end() const noexcept { return _headers.end(); }
};
