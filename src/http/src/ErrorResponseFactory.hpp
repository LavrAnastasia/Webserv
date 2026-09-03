#pragma once

#include "ResolvedRoute.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpStatus.hpp"

class ErrorResponseFactory {
public:
    static HttpResponse create(HttpStatus status);

    static HttpResponse create(HttpStatus status, const ResolvedRoute& route);
};
