#include "http/HttpRequest.hpp"
#include "HeaderFields.hpp"

bool HttpRequest::isPersistent() const {
    return !headers.has(Http::Headers::Connection, Http::Headers::ConnectionOption::Close);
}
