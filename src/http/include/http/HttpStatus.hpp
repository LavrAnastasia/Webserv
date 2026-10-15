#pragma once

#include <optional>
#include <string>

enum class HttpStatus {
    Continue = 100,

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
    RequestTimeout = 408,
    Conflict = 409,
    PayloadTooLarge = 413,
    UriTooLong = 414,
    ExpectationFailed = 417,
    RequestHeaderFieldsTooLarge = 431,

    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503,
    GatewayTimeout = 504,
    HttpVersionNotSupported = 505,
};

namespace Http::Status {
    bool isRedirect(HttpStatus status);
    bool isInformational(HttpStatus status);
    bool isError(HttpStatus status);

    std::optional<HttpStatus> fromCode(int code);
    std::string toString(HttpStatus status);
} // namespace Http::Status
