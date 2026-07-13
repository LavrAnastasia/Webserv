#include <algorithm>

#include "HeadersParser.hpp"
#include "HttpHeadersUtils.hpp"
#include "HttpSyntax.hpp"
#include "HttpUtils.hpp"
#include "http/HttpHeaders.hpp"

HeadersParser::HeadersParser(const std::string& headersBlock) : headersBlock_(headersBlock), headers_() {
}

std::optional<HttpHeaders> HeadersParser::parse(const std::string& headersBlock) {
    return HeadersParser{headersBlock}.run();
}

namespace {
    bool isValidHeaderName(const std::string& name) {
        return !name.empty() && std::ranges::all_of(name, [](char c) {
            unsigned char uc = static_cast<unsigned char>(c);

            return uc > 32 && uc != 127 && std::string_view(":()<>@,;\\\"/[]?={}").find(c) == std::string_view::npos;
        });
    }

    bool isValidHeaderValue(const std::string& value) {
        std::size_t index = 0;

        while (index < value.size()) {
            char c = value[index];
            unsigned char uc = static_cast<unsigned char>(c);

            if ((uc < 32 && c != Http::Syntax::HTAB) || uc == 127)
                return false;

            ++index;
        }

        return true;
    }

    bool canStoreHeader(const HttpHeaders& headers, const std::string& key) {
        if (headers.has(key))
            return false;

        if (key == Http::Headers::ContentLength && headers.has(std::string(Http::Headers::TransferEncoding)))
            return false;

        if (key == Http::Headers::TransferEncoding && headers.has(std::string(Http::Headers::ContentLength)))
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

    std::string name = line.substr(0, colon);
    std::string value = Http::Ascii::trim(line.substr(colon + 1));

    if (!isValidHeaderName(name))
        return false;

    if (!isValidHeaderValue(value))
        return false;

    std::string key = Http::Ascii::tolower(name);

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
