#pragma once

#include <http/HttpHeaders.hpp>
#include <http/HttpStatus.hpp>
#include <http/ResponseBody.hpp>

struct HttpResponse {
    HttpStatus status;
    HttpHeaders headers = {};
    ResponseBody body = {};
};
