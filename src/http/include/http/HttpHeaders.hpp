#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

class HttpHeaders {
private:
    std::map<std::string, std::string> _headers;

public:
    static bool equals(std::string_view a, std::string_view b);

    bool set(const std::string& name, const std::string& value);
    bool has(const std::string& name) const;
    std::optional<std::string> get(const std::string& name) const;
};
