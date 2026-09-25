#pragma once

#include <string>

#include "http/HttpHeaders.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpStatus.hpp"

class HttpResponseFactory {
public:
    static HttpResponse create(HttpStatus status);
    static HttpResponse create(HttpStatus status, HttpHeaders headers);
    static HttpResponse create(HttpStatus status, HttpHeaders headers, std::string body);
    static HttpResponse create(HttpStatus status, std::string body, std::string contentType);
};
