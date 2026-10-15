#pragma once

#include <string>

#include "http/HttpHeaders.hpp"
#include "http/HttpMethod.hpp"

struct HttpRequest {
    HttpMethod method;

    std::string path;
    std::string query;
    std::string version;
    HttpHeaders headers;
    std::string body;

    bool isPersistent() const;
};
