#include "http/HttpStatus.hpp"
#include "ErrorStatus.hpp"

namespace {
    constexpr std::string_view unknownStatus = "Unknown Status";
}

namespace Http::Status {
    std::string toString(HttpStatus status) {
        switch (status) {
            case HttpStatus::OK:
                return "OK";

            case HttpStatus::Created:
                return "Created";

            case HttpStatus::NoContent:
                return "No Content";

            case HttpStatus::MovedPermanently:
                return "Moved Permanently";

            case HttpStatus::Found:
                return "Found";

            case HttpStatus::SeeOther:
                return "See Other";

            case HttpStatus::TemporaryRedirect:
                return "Temporary Redirect";

            case HttpStatus::PermanentRedirect:
                return "Permanent Redirect";

            case HttpStatus::BadRequest:
                return "Bad Request";

            case HttpStatus::Forbidden:
                return "Forbidden";

            case HttpStatus::NotFound:
                return "Not Found";

            case HttpStatus::MethodNotAllowed:
                return "Method Not Allowed";

            case HttpStatus::RequestTimeout:
                return "Request Timeout";

            case HttpStatus::Conflict:
                return "Conflict";

            case HttpStatus::PayloadTooLarge:
                return "Payload Too Large";

            case HttpStatus::UriTooLong:
                return "URI Too Long";

            case HttpStatus::RequestHeaderFieldsTooLarge:
                return "Request Header Fields Too Large";

            case HttpStatus::InternalServerError:
                return "Internal Server Error";

            case HttpStatus::NotImplemented:
                return "Not Implemented";

            case HttpStatus::BadGateway:
                return "Bad Gateway";

            case HttpStatus::ServiceUnavailable:
                return "Service Unavailable";

            case HttpStatus::GatewayTimeout:
                return "Gateway Timeout";

            case HttpStatus::HttpVersionNotSupported:
                return "HTTP Version Not Supported";

            case HttpStatus::NotModified:
                return "Not Modified";
        }

        return std::string(unknownStatus);
    }

    std::optional<HttpStatus> fromCode(int code) {
        const HttpStatus status = static_cast<HttpStatus>(code);

        return toString(status) == unknownStatus ? std::nullopt : std::optional<HttpStatus>(status);
    }

    HttpStatus from(const std::error_code& error) {
        if (error == std::errc::permission_denied || error == std::errc::operation_not_permitted) {
            return HttpStatus::Forbidden;
        }

        if (error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory) {
            return HttpStatus::NotFound;
        }

        return HttpStatus::InternalServerError;
    }
} // namespace Http::Status
