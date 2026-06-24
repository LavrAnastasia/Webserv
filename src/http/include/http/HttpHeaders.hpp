#pragma once

#include <map>
#include <optional>
#include <string>

class HttpHeaders {
private:
    std::map<std::string, std::string> _headers;

public:
    void set(const std::string& name, const std::string& value);
    bool has(const std::string& name) const;
    std::optional<std::string> get(const std::string& name) const;

    bool parseHeaderLine(const std::string& line);
    bool parseHeadersBlock(const std::string& headersBlock);
};
