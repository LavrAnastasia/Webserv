#include <charconv>
#include <optional>
#include <string>
#include <string_view>

#include "HeaderFields.hpp"
#include "HeadersParser.hpp"
#include "HttpSyntax.hpp"
#include "http/CgiResponseParser.hpp"
#include "http/HttpHeaders.hpp"
#include "http/HttpStatus.hpp"

namespace {
    constexpr std::size_t kStatusCodeSize = 3;

    struct Sections {
        std::string headers;
        std::string_view body;
    };

    std::optional<Sections> split(std::string_view output) {
        Sections sections;
        std::size_t start = 0;

        while (true) {
            const std::size_t end = output.find(Http::Syntax::LF, start);

            if (end == std::string_view::npos) {
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

        if (value->size() < kStatusCodeSize) {
            return std::nullopt;
        }

        const char* begin = value->data();
        const char* end = begin + kStatusCodeSize;
        int code{};

        const auto [ptr, ec] = std::from_chars(begin, end, code);

        if (ec != std::errc{} || ptr != end) {
            return std::nullopt;
        }

        return Http::Status::fromCode(code);
    }

    bool isForwarded(std::string_view name) {
        return !HttpHeaders::equals(name, Http::Headers::Status) &&
            !HttpHeaders::equals(name, Http::Headers::Connection);
    }
} // namespace

std::optional<HttpResponse> CgiResponseParser::parse(std::string_view output) {
    const std::optional<Sections> sections = split(output);

    if (!sections) {
        return std::nullopt;
    }

    const std::optional<HttpHeaders> headers = HeadersParser::parse(sections->headers);

    if (!headers) {
        return std::nullopt;
    }

    const std::optional<HttpStatus> status = parseStatus(*headers);

    if (!status) {
        return std::nullopt;
    }

    HttpResponse response{};

    response.status = *status;

    for (const auto& [name, value] : *headers) {
        if (isForwarded(name)) {
            response.headers.set(name, value);
        }
    }

    response.body = std::string(sections->body);

    return response;
}
