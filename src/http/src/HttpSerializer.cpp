#include <ctime>
#include <stdexcept>

#include "HttpHeadersUtils.hpp"
#include "HttpStatusUtils.hpp"
#include "HttpSyntax.hpp"
#include "HttpUtils.hpp"
#include "http/HttpSerializer.hpp"

namespace {
    std::string httpDate() {
        const std::time_t now = std::time(nullptr);

        std::tm time{};

        if (gmtime_r(&now, &time) == nullptr) {
            throw std::runtime_error("failed to create HTTP date");
        }

        char buffer[64]{};

        const std::size_t size = std::strftime(
            buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &time
        ); //TOD0: replace with fixed RFC 7231 day/month tables

        if (size == 0) {
            throw std::runtime_error("failed to format HTTP date");
        }

        return std::string(buffer, size);
    }

    bool statusForbidsBody(HttpStatus status) {
        const int statusCode = static_cast<int>(status);

        return (statusCode >= 100 && statusCode < 200) || status == HttpStatus::NoContent ||
            status == HttpStatus::NotModified;
    }

    void appendHeader(std::string& output, std::string_view name, std::string_view value) {
        output.append(name)
            .append(1, Http::Syntax::HeaderKeyEnd)
            .append(1, Http::Syntax::SP)
            .append(value)
            .append(Http::Syntax::CRLF);
    }
} // namespace

std::string HttpSerializer::serialize(const HttpResponse& response, bool headersOnly) {
    const int statusCode = static_cast<int>(response.status);
    const std::string& body = response.body;
    const bool bodyForbidden = statusForbidsBody(response.status);

    std::string output;
    output.reserve(256 + body.size());

    output.append(Http::Protocol::Name)
        .append(1, Http::Protocol::VersionSeparator)
        .append(Http::Protocol::Version)
        .append(1, Http::Syntax::SP)
        .append(std::to_string(statusCode))
        .append(1, Http::Syntax::SP)
        .append(Http::Status::toString(response.status))
        .append(Http::Syntax::CRLF);

    for (const auto& [name, value] : response.headers) {
        if (HttpHeaders::equals(name, Http::Headers::ContentLength)) {
            continue;
        }

        if (HttpHeaders::equals(name, Http::Headers::TransferEncoding)) {
            throw std::invalid_argument("transfer encoding is not supported");
        }

        appendHeader(output, name, value);
    }

    if (!response.headers.has(Http::Headers::Date)) {
        appendHeader(output, Http::Headers::Date, httpDate());
    }

    if (!response.headers.has(Http::Headers::Server)) {
        appendHeader(output, Http::Headers::Server, Http::Server::Name);
    }

    if (!bodyForbidden) {
        appendHeader(output, Http::Headers::ContentLength, std::to_string(body.size()));
    }

    output.append(Http::Syntax::CRLF);

    if (!headersOnly && !bodyForbidden) {
        output.append(body);
    }

    return output;
}
