#include "http/RequestDispatcher.hpp"

#include "CgiHandler.hpp"
#include "ErrorResponseFactory.hpp"
#include "RedirectHandler.hpp"
#include "Router.hpp"
#include "StaticHandler.hpp"
#include "UploadHandler.hpp"

HandlerResult RequestDispatcher::dispatch(const HttpRequest& request, const ServerConfig& server) {
    const std::optional<ResolvedRoute> route = Router::resolve(request, server);

    if (!route) {
        return ErrorResponseFactory::create(HttpStatus::NotFound, server);
    }

    if (route->redirect) {
        return RedirectHandler::handle(*route->redirect, *route);
    }

    if (!route->allowedMethods.contains(request.method)) {
        return ErrorResponseFactory::create(HttpStatus::MethodNotAllowed, *route);
    }

    if (request.body.size() > route->clientMaxBodySize) {
        return ErrorResponseFactory::create(HttpStatus::PayloadTooLarge, *route);
    }

    if (route->cgi) {
        return CgiHandler::handle(request, *route);
    }

    if (route->upload && request.method == HttpMethod::Post) {
        return UploadHandler::handle(request, *route);
    }

    if (request.method == HttpMethod::Post) {
        return ErrorResponseFactory::create(HttpStatus::MethodNotAllowed, *route);
    }

    return StaticHandler::handle(request, *route);
}

HttpResponse RequestDispatcher::fail(HttpStatus status, const ServerConfig& server) {
    return ErrorResponseFactory::create(status, server);
}
