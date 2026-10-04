#include "HeadersParser.hpp"
#include "HeaderFields.hpp"
#include "HttpSyntax.hpp"
#include "HttpUtils.hpp"

#include <algorithm>

HeadersParser::HeadersParser(const std::string& headersBlock) : headersBlock_(headersBlock), headers_() {
}

std::optional<HttpHeaders> HeadersParser::parse(const std::string& headersBlock) {
    return HeadersParser{headersBlock}.run();
}

namespace {

    bool isHostNameCharacter(char c) {
        return Http::Ascii::isalnum(c) || Http::Syntax::Host::NameSymbols.find(c) != std::string_view::npos;
    }

    bool isValidHostName(std::string_view name) {
        for (std::size_t index = 0; index < name.size(); ++index) {
            if (name[index] == Http::Syntax::Host::PercentEncodingPrefix) {
                if (name.size() - index < 3 || !Http::Ascii::isxdigit(name[index + 1]) ||
                    !Http::Ascii::isxdigit(name[index + 2])) {
                    return false;
                }
                index += 2;
            } else if (!isHostNameCharacter(name[index])) {
                return false;
            }
        }
        return true;
    }

    bool isValidIpLiteral(std::string_view literal) {
        return !literal.empty() && std::ranges::all_of(literal, [](char c) {
            return isHostNameCharacter(c) || c == Http::Syntax::Host::PortSeparator;
        });
    }

    bool isValidHost(std::string_view value) {
        if (value.starts_with(Http::Syntax::Host::LiteralOpen)) {
            const std::size_t close = value.find(Http::Syntax::Host::LiteralClose);
            if (close == std::string_view::npos || !isValidIpLiteral(value.substr(1, close - 1))) {
                return false;
            }
            value.remove_prefix(close + 1);
        } else {
            const std::string_view name = value.substr(0, value.find(Http::Syntax::Host::PortSeparator));
            if (!isValidHostName(name)) {
                return false;
            }
            value.remove_prefix(name.size());
        }

        return value.empty() ||
            (value.front() == Http::Syntax::Host::PortSeparator &&
             std::ranges::all_of(value.substr(1), Http::Ascii::isdigit));
    }

    bool isUnique(std::string_view name) {
        return HttpHeaders::equals(name, Http::Headers::ContentLength) ||
            HttpHeaders::equals(name, Http::Headers::Host);
    }

    bool canStoreHeader(const HttpHeaders& headers, const std::string& name) {
        if (isUnique(name) && headers.has(name))
            return false;

        if (HttpHeaders::equals(name, Http::Headers::ContentLength) &&
            headers.has(std::string(Http::Headers::TransferEncoding)))
            return false;

        if (HttpHeaders::equals(name, Http::Headers::TransferEncoding) &&
            headers.has(std::string(Http::Headers::ContentLength)))
            return false;

        return true;
    }

    bool isValidName(const std::string& name) {
        return !name.empty() && std::ranges::all_of(name, [](char c) {
            const unsigned char uc = static_cast<unsigned char>(c);

            return uc > 32 && uc < 127 && std::string_view(":()<>@,;\\\"/[]?={}").find(c) == std::string_view::npos;
        });
    }

    bool isValidValue(const std::string& value) {
        std::size_t index = 0;

        while (index < value.size()) {
            const char c = value[index];
            const unsigned char uc = static_cast<unsigned char>(c);

            if ((uc < 32 && c != Http::Syntax::HTAB) || uc == 127) {
                return false;
            }

            ++index;
        }

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

    if (!isValidName(key))
        return false;

    if (!isValidValue(value))
        return false;

    if (HttpHeaders::equals(key, Http::Headers::Host) && !isValidHost(value))
        return false;

    if (!canStoreHeader(headers_, key))
        return false;

    headers_.add(key, value);

    return true;
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
