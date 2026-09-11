#include "RedirectHandler.hpp"
#include "ErrorResponseFactory.hpp"
#include "HttpHeaders.hpp"
#include "HttpResponseFactory.hpp"
#include "MimeTypes.hpp"

#include "http/HttpStatus.hpp"

HttpResponse RedirectHandler::handle(const RedirectConfig& redirect, const ResolvedRoute& route) {
    HttpResponse response{};

    response.status = redirect.status;

    if (Http::Status::isRedirect(redirect.status)) {
        response.headers.set(std::string(Http::Headers::Location), *redirect.target);

        return response;
    }

    if (redirect.target) {
        return HttpResponseFactory::create(redirect.status, *redirect.target, std::string(Http::Mime::Text));
    }

    if (Http::Status::isError(redirect.status)) {
        return ErrorResponseFactory::create(redirect.status, route);
    }

    return response;
}
