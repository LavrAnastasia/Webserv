#include "http/HttpHeaders.hpp"
#include "HttpUtils.hpp"

bool HttpHeaders::set(const std::string& name, const std::string& value) {
    const std::string key = Http::Ascii::tolower(name);

    return _headers.emplace(key, value).second;
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

void HttpHeaders::setOrReplace(std::string name, std::string value) {
    const std::string key = Http::Ascii::tolower(name);

    _headers[key] = std::move(value);
}

bool HttpHeaders::remove(std::string_view name) {
    const std::string key = Http::Ascii::tolower(std::string(name));

    return _headers.erase(key) != 0;
}

const HttpHeaders::Storage& HttpHeaders::entries() const noexcept {
    return _headers;
}
