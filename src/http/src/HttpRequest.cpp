#include "http/HttpRequest.hpp"
#include "HttpHeaders.hpp"

#include <string>

bool HttpRequest::isPersistent() const {
    const std::optional<std::string> value = headers.get(std::string(Http::Headers::Connection));

    return !value || !HttpHeaders::equals(*value, Http::Headers::ConnectionOption::Close);
}
