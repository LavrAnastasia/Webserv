#include "RequestDispatcher.hpp"

#include <utility>

#include "CgiHandler.hpp"
#include "ErrorResponseFactory.hpp"
#include "HeaderFields.hpp"
#include "RedirectHandler.hpp"
#include "Router.hpp"
#include "StaticHandler.hpp"
#include "UploadHandler.hpp"

DispatchResult RequestDispatcher::dispatch(
    HttpRequest request,
    std::optional<std::size_t> length,
    const ServerConfig& server,
    const ConnectionInfo& connectionInfo
) {
    std::optional<ResolvedRoute> route = Router::resolve(request, server);

    if (route && length && *length > route->clientMaxBodySize) {
        return ErrorResponseFactory::create(HttpStatus::PayloadTooLarge, *route);
    }

    const auto expect = request.headers.get(Http::Headers::Expect);
    if (expect && !HttpHeaders::equals(*expect, Http::Headers::Expectation::Continue)) {
        return route ? ErrorResponseFactory::create(HttpStatus::ExpectationFailed, *route)
                     : ErrorResponseFactory::create(HttpStatus::ExpectationFailed, Router::mapErrorPages(server));
    }

    if (!route) {
        return ErrorResponseFactory::create(HttpStatus::NotFound, Router::mapErrorPages(server));
    }

    if (route->redirect) {
        return RedirectHandler::handle(*route->redirect, *route);
    }

    if (!route->allowedMethods.contains(request.method)) {
        return ErrorResponseFactory::create(HttpStatus::MethodNotAllowed, *route);
    }

    if (route->cgi) {
        return CgiHandler::start(std::move(request), std::move(*route), connectionInfo, length);
    }

    if (route->upload && request.method == HttpMethod::Post) {
        return UploadHandler::start(std::move(request), std::move(*route));
    }

    return StaticHandler::start(std::move(request), std::move(*route));
}
