#pragma once

enum class HttpStatus {
    OK = 200,
    Created = 201,
    NoContent = 204,

    MovedPermanently = 301,
    Found = 302,
    SeeOther = 303,
    NotModified = 304,
    TemporaryRedirect = 307,
    PermanentRedirect = 308,

    BadRequest = 400,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    PayloadTooLarge = 413,
    UriTooLong = 414,

    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    GatewayTimeout = 504,
    HttpVersionNotSupported = 505,
};

namespace Http::Status {
    constexpr bool isRedirect(HttpStatus status) {
        switch (status) {
            case HttpStatus::MovedPermanently:
            case HttpStatus::Found:
            case HttpStatus::SeeOther:
            case HttpStatus::TemporaryRedirect:
            case HttpStatus::PermanentRedirect:
                return true;
            default:
                return false;
        }
    }
} // namespace Http::Status
