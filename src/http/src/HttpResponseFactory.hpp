#pragma once

#include <string>

#include "http/HttpResponse.hpp"
#include "http/HttpStatus.hpp"

class HttpResponseFactory {
public:
    static HttpResponse create(HttpStatus status, std::string body, std::string contentType);
};
