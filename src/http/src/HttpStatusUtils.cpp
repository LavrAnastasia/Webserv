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
        }

        return "Unknown Status";
    }
} // namespace Http::Status
