#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

class HttpHeaders {
private:
    std::map<std::string, std::string> _headers;

public:
    using Storage = std::map<std::string, std::string>;

    bool set(const std::string& name, const std::string& value);
    void setOrReplace(std::string name, std::string value);
    bool remove(std::string_view name);
    bool has(const std::string& name) const;
    std::optional<std::string> get(const std::string& name) const;
    const Storage& entries() const noexcept;
};
