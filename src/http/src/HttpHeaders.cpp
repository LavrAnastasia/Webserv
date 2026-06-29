#include "http/HttpHeaders.hpp"
#include "http/HttpUtils.hpp"

void HttpHeaders::set(const std::string& name, const std::string& value) {
    _headers[toLowerAscii(name)] = value;
}

bool HttpHeaders::has(const std::string& name) const {
    return _headers.find(toLowerAscii(name)) != _headers.end();
}

std::optional<std::string> HttpHeaders::get(const std::string& name) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(toLowerAscii(name));
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
