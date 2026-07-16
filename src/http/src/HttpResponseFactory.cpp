#include "HttpResponseFactory.hpp"
#include "HttpHeadersUtils.hpp"

HttpResponse HttpResponseFactory::create(HttpStatus status, std::string body, std::string contentType) {
    HttpResponse response;

    response.status = status;
    response.headers.set(std::string(Http::Headers::ContentType), std::move(contentType));
    response.headers.set(std::string(Http::Headers::ContentLength), std::to_string(body.size()));
    response.body = std::move(body);

    return response;
}
