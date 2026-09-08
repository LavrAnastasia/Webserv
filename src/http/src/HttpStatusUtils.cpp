#include "HttpStatusUtils.hpp"

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

            case HttpStatus::BadRequest:
                return "Bad Request";

            case HttpStatus::Forbidden:
                return "Forbidden";

            case HttpStatus::NotFound:
                return "Not Found";

            case HttpStatus::MethodNotAllowed:
                return "Method Not Allowed";

            case HttpStatus::PayloadTooLarge:
                return "Payload Too Large";

            case HttpStatus::UriTooLong:
                return "URI Too Long";

            case HttpStatus::InternalServerError:
                return "Internal Server Error";

            case HttpStatus::NotImplemented:
                return "Not Implemented";

            case HttpStatus::BadGateway:
                return "Bad Gateway";

            case HttpStatus::GatewayTimeout:
                return "Gateway Timeout";

            case HttpStatus::HttpVersionNotSupported:
                return "HTTP Version Not Supported";

            case HttpStatus::NotModified:
                return "Not Modified";
        }

        return "Unknown Status";
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
