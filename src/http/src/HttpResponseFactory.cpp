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
    return HttpResponse{.status = status, .headers = std::move(headers), .body = ResponseBody(std::move(body))};
}

HttpResponse HttpResponseFactory::create(HttpStatus status, std::string body, std::string contentType) {
    return create(status, ResponseBody(std::move(body)), std::move(contentType));
}

HttpResponse HttpResponseFactory::create(HttpStatus status, ResponseBody body, std::string contentType) {
    return HttpResponse{
        .status = status, .headers = HttpHeaders{{Http::Headers::ContentType, contentType}}, .body = std::move(body)
    };
}
