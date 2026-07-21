#include <stdexcept>

#include "HttpUtils.hpp"
#include "http/HttpResponse.hpp"

// namespace {
//     std::string reasonPhraseFor(int statusCode) {
//         switch (statusCode) {
//             case 100:
//                 return "Continue";
//             case 101:
//                 return "Switching Protocols";

//             case 200:
//                 return "OK";
//             case 201:
//                 return "Created";
//             case 202:
//                 return "Accepted";
//             case 204:
//                 return "No Content";

//             case 300:
//                 return "Multiple Choices";
//             case 301:
//                 return "Moved Permanently";
//             case 302:
//                 return "Found";
//             case 303:
//                 return "See Other";
//             case 304:
//                 return "Not Modified";
//             case 307:
//                 return "Temporary Redirect";
//             case 308:
//                 return "Permanent Redirect";

//             case 400:
//                 return "Bad Request";
//             case 401:
//                 return "Unauthorized";
//             case 403:
//                 return "Forbidden";
//             case 404:
//                 return "Not Found";
//             case 405:
//                 return "Method Not Allowed";
//             case 408:
//                 return "Request Timeout";
//             case 409:
//                 return "Conflict";
//             case 411:
//                 return "Length Required";
//             case 413:
//                 return "Content Too Large";
//             case 414:
//                 return "URI Too Long";
//             case 415:
//                 return "Unsupported Media Type";
//             case 422:
//                 return "Unprocessable Content";
//             case 429:
//                 return "Too Many Requests";
//             case 431:
//                 return "Request Header Fields Too Large";

//             case 500:
//                 return "Internal Server Error";
//             case 501:
//                 return "Not Implemented";
//             case 502:
//                 return "Bad Gateway";
//             case 503:
//                 return "Service Unavailable";
//             case 504:
//                 return "Gateway Timeout";
//             case 505:
//                 return "HTTP Version Not Supported";

//             default:
//                 return "Unknown Status";
//         }
//     }

//     bool isRedirectStatus(int statusCode) {
//         return statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308;
//     }

//     bool isManagedHeader(const std::string& key) {
//         return key == Http::Header::ContentLength || key == Http::Header::TransferEncoding ||
//             key == Http::Header::Connection;
//     }
// } // namespace

void HttpResponse::setStatus(int statusCode) {
    if (statusCode < 100 || statusCode > 599) {
        throw std::invalid_argument("invalid HTTP status code");
    }

    // setStatus(statusCode, reasonPhraseFor(statusCode));
}

void HttpResponse::setStatus(int statusCode, std::string reasonPhrase) {
    if (statusCode < 100 || statusCode > 599) {
        throw std::invalid_argument("invalid HTTP status code");
    }

    if (reasonPhrase.find_first_of("\r\n") != std::string::npos) {
        throw std::invalid_argument("invalid HTTP reason phrase");
    }

    _statusCode = statusCode;
    _reasonPhrase = std::move(reasonPhrase);
}

int HttpResponse::statusCode() const noexcept {
    return _statusCode;
}

std::string_view HttpResponse::reasonPhrase() const noexcept {
    return _reasonPhrase;
}

void HttpResponse::setHeader(std::string name, std::string value) {
    if (!Http::Header::isValidName(name) || !Http::Header::isValidValue(value)) {
        throw std::invalid_argument("invalid HTTP response header");
    }

    const std::string key = Http::Ascii::tolower(name);

    // if (isManagedHeader(key)) {
    //     throw std::invalid_argument(
    //         "header is managed by HttpResponse "
    //         "or HttpSerializer"
    //     );
    // }

    _headers.setOrReplace(std::move(name), std::move(value));
}

bool HttpResponse::removeHeader(const std::string& name) {
    return _headers.remove(name);
}

bool HttpResponse::hasHeader(const std::string& name) const {
    return _headers.has(name);
}

std::optional<std::string> HttpResponse::header(const std::string& name) const {
    return _headers.get(name);
}

const HttpHeaders& HttpResponse::headers() const noexcept {
    return _headers;
}

void HttpResponse::setBody(std::string body) {
    _body = std::move(body);
}

void HttpResponse::setBody(std::string body, std::string contentType) {
    setHeader("Content-Type", std::move(contentType));
    _body = std::move(body);
}

void HttpResponse::clearBody() noexcept {
    _body.clear();
}

std::string_view HttpResponse::body() const noexcept {
    return _body;
}

void HttpResponse::setConnectionPreference(ConnectionPreference preference) noexcept {
    _connectionPreference = preference;
}

ConnectionPreference HttpResponse::connectionPreference() const noexcept {
    return _connectionPreference;
}

bool HttpResponse::shouldCloseConnection() const noexcept {
    return _connectionPreference == ConnectionPreference::Close;
}

HttpResponse HttpResponse::error(int statusCode, std::string body) {
    if (statusCode < 400 || statusCode > 599) {
        throw std::invalid_argument(
            "error response requires "
            "a 4xx or 5xx status"
        );
    }

    HttpResponse response;
    response.setStatus(statusCode);

    if (body.empty()) {
        body = std::to_string(statusCode);
        body += ' ';
        body += response.reasonPhrase();
        body += '\n';
    }

    response.setBody(std::move(body), "text/plain; charset=utf-8");

    return response;
}

HttpResponse HttpResponse::redirect(int statusCode, std::string target) {
    // if (!isRedirectStatus(statusCode)) {
    //     throw std::invalid_argument("unsupported redirect status code");
    // }

    HttpResponse response;
    response.setStatus(statusCode);

    std::string body = "Redirecting to " + target + "\n";

    response.setHeader("Location", std::move(target));

    response.setBody(std::move(body), "text/plain; charset=utf-8");

    return response;
}
