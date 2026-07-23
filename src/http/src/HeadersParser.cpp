#include "HeadersParser.hpp"
#include "HttpHeadersUtils.hpp"
#include "HttpSyntax.hpp"
#include "HttpUtils.hpp"

HeadersParser::HeadersParser(const std::string& headersBlock) : headersBlock_(headersBlock), headers_() {
}

std::optional<HttpHeaders> HeadersParser::parse(const std::string& headersBlock) {
    return HeadersParser{headersBlock}.run();
}

namespace {

    bool canStoreHeader(const HttpHeaders& headers, const std::string& name) {
        if (headers.has(name))
            return false;

        if (HttpHeaders::equals(name, Http::Headers::ContentLength) &&
            headers.has(std::string(Http::Headers::TransferEncoding)))
            return false;

        if (HttpHeaders::equals(name, Http::Headers::TransferEncoding) &&
            headers.has(std::string(Http::Headers::ContentLength)))
            return false;

        return true;
    }

} // namespace

bool HeadersParser::parseHeaderLine(const std::string& line) {
    if (!line.empty() && (line[0] == Http::Syntax::SP || line[0] == Http::Syntax::HTAB))
        return false;

    std::size_t colon = line.find(Http::Syntax::HeaderKeyEnd);
    if (colon == std::string::npos)
        return false;

    std::string key = line.substr(0, colon);
    std::string value = Http::Ascii::trim(line.substr(colon + 1));

    if (!Http::Header::isValidName(key))
        return false;

    if (!Http::Header::isValidValue(value))
        return false;

    if (!canStoreHeader(headers_, key))
        return false;

    return headers_.set(key, value);
}

std::optional<HttpHeaders> HeadersParser::run() {
    std::size_t start = 0;

    while (start < headersBlock_.size()) {
        std::size_t end = headersBlock_.find(Http::Syntax::CRLF, start);

        std::string line;
        if (end == std::string::npos) {
            line = headersBlock_.substr(start);
            start = headersBlock_.size();
        } else {
            line = headersBlock_.substr(start, end - start);
            start = end + Http::Syntax::CRLF.size();
        }

        if (!parseHeaderLine(line))
            return std::nullopt;
    }

    return headers_;
}
