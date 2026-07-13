#include "HttpResponseFactory.hpp"

HttpResponse HttpResponseFactory::create(HttpStatus status, std::string body, std::string contentType) {
    HttpResponse response;

    response.status = status;
    // TODO: replace to headers utils
    response.headers.set("Content-Type", std::move(contentType));
    response.headers.set("Content-Length", std::to_string(body.size()));
    response.body = std::move(body);

    return response;
}
