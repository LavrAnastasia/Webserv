#pragma once

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class HttpHeaders {
private:
    std::vector<std::pair<std::string, std::string>> _headers;

public:
    HttpHeaders() = default;
    explicit HttpHeaders(std::initializer_list<std::pair<std::string_view, std::string_view>> fields);

    static bool equals(std::string_view a, std::string_view b);

    void add(const std::string& name, const std::string& value);
    void set(const std::string& name, const std::string& value);
    void erase(std::string_view name);
    bool has(std::string_view name) const;
    bool has(std::string_view name, std::string_view token) const;
    std::optional<std::string> get(std::string_view name) const;
    std::string serialize() const;
};
