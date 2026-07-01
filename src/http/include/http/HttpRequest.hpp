#pragma once

#include <optional>
#include <string>
#include <vector>


#include "http/HttpHeaders.hpp"
#include "http/HttpMethod.hpp"

struct HttpRequest {
    HttpMethod method;

    std::string target;
    std::string path;
    std::string query;
    std::string version;
    HttpHeaders headers;
    std::string body;
};
