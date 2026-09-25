#include <utility>

#include "HeaderFields.hpp"
#include "HttpResponseFactory.hpp"

HttpResponse HttpResponseFactory::create(HttpStatus status) {
    return HttpResponse{.status = status};
}

HttpResponse HttpResponseFactory::create(HttpStatus status, HttpHeaders headers) {
    return HttpResponse{.status = status, .headers = std::move(headers)};
}

HttpResponse HttpResponseFactory::create(HttpStatus status, HttpHeaders headers, std::string body) {
    return HttpResponse{.status = status, .headers = std::move(headers), .body = std::move(body)};
}

HttpResponse HttpResponseFactory::create(HttpStatus status, std::string body, std::string contentType) {
    return create(status, HttpHeaders{{Http::Headers::ContentType, contentType}}, std::move(body));
}
