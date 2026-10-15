#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "CgiResponseParser.hpp"
#include "HeaderFields.hpp"
#include "HeadersParser.hpp"
#include "HttpSyntax.hpp"
#include "http/HttpHeaders.hpp"
#include "http/HttpStatus.hpp"

namespace {
    constexpr std::size_t kHeaderLimit = 16 * 1024; // 16 KB
    constexpr std::size_t kStatusCodeSize = 3;
    constexpr int kMinStatus = 200;
    constexpr int kMaxStatus = 599;

    struct Sections {
        std::string headers;
        std::string_view body;
    };

    std::optional<Sections> split(std::string_view output) {
        Sections sections;
        std::size_t start = 0;

        while (true) {
            const std::size_t end = output.find(Http::Syntax::LF, start);

            if (end == std::string_view::npos || end >= kHeaderLimit) {
                return std::nullopt;
            }

            std::string_view line = output.substr(start, end - start);

            if (line.ends_with(Http::Syntax::CR)) {
                line.remove_suffix(1);
            }

            start = end + 1;

            if (line.empty()) {
                sections.body = output.substr(start);

                return sections;
            }

            sections.headers.append(line).append(Http::Syntax::CRLF);
        }
    }

    std::optional<HttpStatus> parseStatus(const HttpHeaders& headers) {
        const std::optional<std::string> value = headers.get(std::string(Http::Headers::Status));

        if (!value) {
            return headers.has(Http::Headers::Location) ? HttpStatus::Found : HttpStatus::OK;
        }

        if (value->size() < kStatusCodeSize ||
            (value->size() > kStatusCodeSize && (*value)[kStatusCodeSize] != Http::Syntax::SP)) {
            return std::nullopt;
        }

        const char* begin = value->data();
        const char* end = begin + kStatusCodeSize;
        int code{};

        const auto [ptr, ec] = std::from_chars(begin, end, code);

        if (ec != std::errc{} || ptr != end || code < kMinStatus || code > kMaxStatus) {
            return std::nullopt;
        }

        return static_cast<HttpStatus>(code);
    }
} // namespace

std::variant<CgiResponseParser::Incomplete, CgiResponseParser::Invalid, CgiResponseParser::Headers>
CgiResponseParser::parseHeaders(std::string_view output) {
    const std::optional<Sections> sections = split(output);

    if (!sections) {
        if (output.size() >= kHeaderLimit) {
            return Invalid{};
        }
        return Incomplete{};
    }

    if (sections->headers.empty()) {
        return Invalid{};
    }

    std::optional<HttpHeaders> headers = HeadersParser::parse(sections->headers);

    if (!headers || headers->has(Http::Headers::TransferEncoding)) {
        return Invalid{};
    }

    const std::optional<HttpStatus> status = parseStatus(*headers);

    if (!status) {
        return Invalid{};
    }

    std::optional<std::uintmax_t> length;
    if (const auto value = headers->get(Http::Headers::ContentLength)) {
        std::uintmax_t size = 0;
        const char* end = value->data() + value->size();
        const auto result = std::from_chars(value->data(), end, size);
        if (result.ec != std::errc{} || result.ptr != end) {
            return Invalid{};
        }
        length = size;
    }

    for (const auto& name : headers->names()) {
        if (!HttpHeaders::equals(name, Http::Headers::Connection) && headers->has(Http::Headers::Connection, name)) {
            headers->erase(name);
        }
    }

    headers->erase(Http::Headers::Status);
    headers->erase(Http::Headers::Connection);
    headers->erase(Http::Headers::KeepAlive);
    headers->erase(Http::Headers::ProxyConnection);
    headers->erase(Http::Headers::TE);
    headers->erase(Http::Headers::Trailer);
    headers->erase(Http::Headers::Upgrade);

    if (!headers->has(Http::Headers::ContentLength)) {
        length.reset();
    }

    return Headers{
        .response = HttpResponse{.status = *status, .headers = std::move(*headers)},
        .consumed = output.size() - sections->body.size(),
        .length = length
    };
}
