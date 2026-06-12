#pragma once

#include <optional>
#include <string>


#include "http/HttpMethod.hpp"

struct HttpRequest {
    HttpMethod method;

    std::string target;
    std::string path;
    std::string query;
    std::string version;
    // HttpHeaders headers;
    std::string body;
};

std::optional<HttpRequest> parseRequestLine(const std::string& line);
