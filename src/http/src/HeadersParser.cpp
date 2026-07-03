#include "HttpUtils.hpp"
#include "http/HttpHeaders.hpp"

#include "HeadersParser.hpp"

HeadersParser::HeadersParser(const std::string& headersBlock) : headersBlock_(headersBlock), headers_() {
}

std::optional<HttpHeaders> HeadersParser::parse(const std::string& headersBlock) {
    return HeadersParser{headersBlock}.run();
}


namespace {

    bool isValidHeaderName(const std::string& name) {
        if (name.empty())
            return false;

        std::size_t index = 0;
        while (index < name.size()) {
            char c = name[index];
            unsigned char uc = static_cast<unsigned char>(c);

            if (uc <= 32 || uc == 127 || c == ':' || c == '(' || c == ')' || c == '<' || c == '>' || c == '@' ||
                c == ',' || c == ';' || c == '\\' || c == '"' || c == '/' || c == '[' || c == ']' || c == '?' ||
                c == '=' || c == '{' || c == '}') {
                return false;
            }

            ++index;
        }

        return true;
    }

    bool isValidHeaderValue(const std::string& value) {
        std::size_t index = 0;

        while (index < value.size()) {
            char c = value[index];
            unsigned char uc = static_cast<unsigned char>(c);

            if ((uc < 32 && c != '\t') || uc == 127)
                return false;

            ++index;
        }

        return true;
    }

} // namespace

bool HeadersParser::parseHeaderLine(const std::string& line) {
    std::size_t colon = line.find(':');
    if (colon == std::string::npos)
        return false;

    std::string name = line.substr(0, colon);
    std::string value = Http::Ascii::trim(line.substr(colon + 1));

    if (!isValidHeaderName(name))
        return false;

    if (!isValidHeaderValue(value))
        return false;

    return headers_.set(name, value);
}


std::optional<HttpHeaders> HeadersParser::run() {
    std::size_t start = 0;

    while (start < headersBlock_.size()) {
        std::size_t end = headersBlock_.find(Http::Syntax::CrLf, start);

        std::string line;
        if (end == std::string::npos) {
            line = headersBlock_.substr(start);
            start = headersBlock_.size();
        } else {
            line = headersBlock_.substr(start, end - start);
            start = end + Http::Syntax::CrLf.size();
        }

        if (!parseHeaderLine(line))
            return std::nullopt;
    }

    return headers_;
}