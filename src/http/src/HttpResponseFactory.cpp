#include "HttpResponseFactory.hpp"
#include "HttpHeadersUtils.hpp"

HttpResponse HttpResponseFactory::create(HttpStatus status, std::string body, std::string contentType) {
    HttpResponse response;

    response.status = status;
    response.headers.set(std::string(Http::Headers::ContentType), std::move(contentType));
    response.body = std::move(body);

    return response;
}
