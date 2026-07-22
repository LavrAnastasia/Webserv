#pragma once

#include <http/HttpHeaders.hpp>
#include <http/HttpStatus.hpp>
#include <string>

struct HttpResponse {
    HttpStatus status;
    HttpHeaders headers;
    std::string body;
};
