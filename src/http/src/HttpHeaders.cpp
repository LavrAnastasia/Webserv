#include "http/HttpHeaders.hpp"
#include "HttpUtils.hpp"

bool HttpHeaders::set(const std::string& name, const std::string& value) {
    const std::string key = Http::Ascii::tolower(name);

    _headers.emplace(key, value);
    return true;
}

bool HttpHeaders::has(const std::string& name) const {
    const std::string key = Http::Ascii::tolower(name);
    return _headers.find(key) != _headers.end();
}

std::optional<std::string> HttpHeaders::get(const std::string& name) const {
    const std::string key = Http::Ascii::tolower(name);
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it == _headers.end())
        return std::nullopt;
    return it->second;
}
