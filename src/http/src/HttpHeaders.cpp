#include "http/HttpHeaders.hpp"

namespace {
    std::string normalizeHeaderName(const std::string& name) {
        std::string result = name;
        std::size_t index = 0;

        while (index < result.size()) {
            if (result[index] >= 'A' && result[index] <= 'Z') {
                result[index] = static_cast<char>(result[index] - 'A' + 'a');
            }
            ++index;
        }
        return result;
    }
} // namespace

void HttpHeaders::set(const std::string& name, const std::string& value) {
    _headers[normalizeHeaderName(name)] = value;
}

bool HttpHeaders::has(const std::string& name) const {
    return _headers.find(normalizeHeaderName(name)) != _headers.end();
}

std::optional<std::string> HttpHeaders::get(const std::string& name) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(normalizeHeaderName(name));
    if (it == _headers.end())
        return std::nullopt;
    return it->second;
}

bool HttpHeaders::parseHeaderLine(const std::string& line) {
    std::size_t colon = line.find(':');
    if (colon == std::string::npos)
        return false;
    std::string name = line.substr(0, colon);
    std::string value = line.substr(colon + 1);

    if (name.empty())
        return false;
    while (!value.empty() && value[0] == ' ') {
        value.erase(0, 1);
    }
    set(name, value);
    return true;
}

bool HttpHeaders::parseHeadersBlock(const std::string& headersBlock) {
    std::size_t start = 0;

    while (start < headersBlock.size()) {
        std::size_t end = headersBlock.find("\r\n", start);

        std::string line;
        if (end == std::string::npos) {
            line = headersBlock.substr(start);
            start = headersBlock.size();
        } else {
            line = headersBlock.substr(start, end - start);
            start = end + 2;
        }
        if (!parseHeaderLine(line))
            return false;
    }
    return true;
}
