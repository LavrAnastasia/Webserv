#include "http/HttpHeaders.hpp"
#include "HttpUtils.hpp"

#include <string_view>

namespace {
    constexpr std::string_view CONTENT_LENGTH_HEADER = "content-length";
    constexpr std::string_view TRANSFER_ENCODING_HEADER = "transfer-encoding";

    bool canStoreHeader(const std::map<std::string, std::string>& headers, const std::string& key) {
        if (headers.find(key) != headers.end())
            return false;

        if (key == CONTENT_LENGTH_HEADER && headers.find(std::string(TRANSFER_ENCODING_HEADER)) != headers.end())
            return false;

        if (key == TRANSFER_ENCODING_HEADER && headers.find(std::string(CONTENT_LENGTH_HEADER)) != headers.end())
            return false;

        return true;
    }
} // namespace

bool HttpHeaders::set(const std::string& name, const std::string& value) {
    const std::string key = Http::Ascii::tolower(name);

    if (!canStoreHeader(_headers, key))
        return false;

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
