#include "http/RequestHandler.hpp"
#include "ErrorResponseFactory.hpp"
#include "HeaderFields.hpp"
#include "RedirectHandler.hpp"
#include "Router.hpp"
#include "StaticHandler.hpp"

namespace {
    HttpResponse dispatch(const HttpRequest& request, const ServerConfig& server) {
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
            // TODO: WEB-36 CGI Handler
            return ErrorResponseFactory::create(HttpStatus::NotImplemented, *route);
        }

        if (route->upload) {
            // TODO: WEB-37 Upload Handler
            return ErrorResponseFactory::create(HttpStatus::NotImplemented, *route);
        }

        return StaticHandler::handle(request, *route);
    }
} // namespace

HttpResponse RequestHandler::handle(const HttpRequest& request, const ServerConfig& server) {
    HttpResponse response = dispatch(request, server);

    if (!request.isPersistent()) {
        response.headers.set(
            std::string(Http::Headers::Connection), std::string(Http::Headers::ConnectionOption::Close)
        );
    }

    return response;
}

HttpResponse RequestHandler::reject(HttpStatus status, const ServerConfig& server) {
    HttpResponse response = ErrorResponseFactory::create(status, server);

    response.headers.set(std::string(Http::Headers::Connection), std::string(Http::Headers::ConnectionOption::Close));

    return response;
}
