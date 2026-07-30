#include <ctime>
#include <stdexcept>

#include "HttpStatusUtils.hpp"
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

    void appendHeader(std::string& output, const std::string& name, const std::string& value) {
        output.append(name).append(": ").append(value).append("\r\n");
    }
} // namespace

std::string HttpSerializer::serialize(const HttpResponse& response, bool headersOnly) {
    const int statusCode = static_cast<int>(response.status);
    const std::string& body = response.body;

    const bool bodyForbidden = statusForbidsBody(response.status);

    if (bodyForbidden && !body.empty()) {
        throw std::invalid_argument("response status does not permit a body");
    }

    std::string output;
    output.reserve(256 + body.size());

    output.append("HTTP/1.1 ")
        .append(std::to_string(statusCode))
        .append(" ")
        .append(Http::Status::toString(response.status))
        .append("\r\n");

    for (const auto& [name, value] : response.headers) {
        if (HttpHeaders::equals(name, "Content-Length")) {
            continue;
        }

        if (HttpHeaders::equals(name, "Transfer-Encoding")) {
            throw std::invalid_argument("transfer encoding is not supported");
        }

        appendHeader(output, name, value);
    }

    if (!response.headers.has("Date")) {
        appendHeader(output, "Date", httpDate());
    }

    if (!response.headers.has("Server")) {
        appendHeader(output, "Server", "webserv");
    }

    if (!bodyForbidden) {
        appendHeader(output, "Content-Length", std::to_string(body.size()));
    }
    output.append("\r\n");

    if (!headersOnly && !bodyForbidden) {
        output.append(body);
    }

    return output;
}
