#include <array>
#include <ctime>
#include <optional>

#include "HeaderFields.hpp"
#include "HttpSyntax.hpp"

#include "http/HttpSerializer.hpp"
#include "http/HttpStatus.hpp"

namespace {
    constexpr std::string_view serverName = "webserv";

    constexpr std::array<std::string_view, 7> weekDays = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    constexpr std::array<std::string_view, 12> months = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    void appendTwoDigits(std::string& output, int value) {
        output += static_cast<char>('0' + value / 10);
        output += static_cast<char>('0' + value % 10);
    }

    std::optional<std::string> httpDate() {
        const std::time_t now = std::time(nullptr);

        std::tm time{};

        if (gmtime_r(&now, &time) == nullptr) {
            return std::nullopt;
        }

        std::string date;
        date.reserve(29);

        date += weekDays[time.tm_wday];
        date += ", ";
        appendTwoDigits(date, time.tm_mday);
        date += ' ';
        date += months[time.tm_mon];
        date += ' ';
        date += std::to_string(time.tm_year + 1900);
        date += ' ';
        appendTwoDigits(date, time.tm_hour);
        date += ':';
        appendTwoDigits(date, time.tm_min);
        date += ':';
        appendTwoDigits(date, time.tm_sec);
        date += " GMT";

        return date;
    }

    bool statusForbidsBody(HttpStatus status) {
        return Http::Status::isInformational(status) || status == HttpStatus::NoContent ||
            status == HttpStatus::NotModified;
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

    HttpHeaders headers = response.headers;

    headers.erase(Http::Headers::TransferEncoding);

    if (const std::optional<std::string> date = httpDate()) {
        headers.set(std::string(Http::Headers::Date), *date);
    }

    if (!headers.has(Http::Headers::Server)) {
        headers.set(std::string(Http::Headers::Server), std::string(serverName));
    }

    if (bodyForbidden) {
        headers.erase(Http::Headers::ContentLength);
    } else {
        headers.set(std::string(Http::Headers::ContentLength), std::to_string(body.size()));
    }

    output.append(headers.serialize()).append(Http::Syntax::CRLF);

    if (!headersOnly && !bodyForbidden) {
        output.append(body);
    }

    return output;
}
