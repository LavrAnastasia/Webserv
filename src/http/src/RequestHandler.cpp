#include "RequestHandler.hpp"

#include <utility>

#include "ErrorResponseFactory.hpp"

RequestHandler::RequestHandler(HttpRequest request, ResolvedRoute route)
    : request_(std::move(request)), route_(std::move(route)) {
}

RequestHandler::~RequestHandler() = default;

bool RequestHandler::receive(std::string_view) {
    return true;
}

std::size_t RequestHandler::limit() const {
    return route_.clientMaxBodySize;
}

HttpResponse RequestHandler::reject(HttpStatus status) const {
    return ErrorResponseFactory::create(status, route_);
}
