#include "http/RequestHandler.hpp"
#include "ErrorResponseFactory.hpp"
#include "Router.hpp"
#include "StaticHandler.hpp"

HttpResponse RequestHandler::handle(const HttpRequest& request, const ServerConfig& server) {
    const std::optional<ResolvedRoute> route = Router::resolve(request, server);

    if (!route) {
        return ErrorResponseFactory::create(HttpStatus::NotFound);
    }

    if (route->redirect) {
        // TODO: WEB-38 Redirect Handler
        return ErrorResponseFactory::create(HttpStatus::NotImplemented, *route);
    }

    if (!route->allowedMethods.contains(request.method)) {
        return ErrorResponseFactory::create(HttpStatus::MethodNotAllowed, *route);
    }

    if (request.body.size() > route->clientMaxBodySize) {
        return ErrorResponseFactory::create(HttpStatus::PayloadTooLarge, *route);
    }

    if (route->cgi) {
        // TODO: WEB-36 CGI Handler
        return ErrorResponseFactory::create(HttpStatus::NotImplemented, *route);
    }

    if (route->upload) {
        // TODO: WEB-37 Upload
        return ErrorResponseFactory::create(HttpStatus::NotImplemented, *route);
    }

    return StaticHandler::handle(request, *route);
}
