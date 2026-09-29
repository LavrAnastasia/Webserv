#include "http/RequestHandler.hpp"

#include "CgiHandler.hpp"
#include "ErrorResponseFactory.hpp"
#include "HeaderFields.hpp"
#include "RedirectHandler.hpp"
#include "Router.hpp"
#include "StaticHandler.hpp"
#include "UploadHandler.hpp"

namespace {
    HandlerResult dispatch(const HttpRequest& request, const ServerConfig& server) {
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
} // namespace

HandlerResult RequestHandler::handle(const HttpRequest& request, const ServerConfig& server) {
    HandlerResult result = dispatch(request, server);
    HttpResponse* response = std::get_if<HttpResponse>(&result);

    if (response != nullptr && !request.isPersistent()) {
        response->headers.set(
            std::string(Http::Headers::Connection), std::string(Http::Headers::ConnectionOption::Close)
        );
    }

    return result;
}

HttpResponse RequestHandler::reject(HttpStatus status, const ServerConfig& server) {
    HttpResponse response = ErrorResponseFactory::create(status, server);

    response.headers.set(std::string(Http::Headers::Connection), std::string(Http::Headers::ConnectionOption::Close));

    return response;
}
