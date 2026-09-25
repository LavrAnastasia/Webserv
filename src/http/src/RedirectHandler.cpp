#include "RedirectHandler.hpp"
#include "ErrorResponseFactory.hpp"
#include "HeaderFields.hpp"
#include "HttpResponseFactory.hpp"
#include "MimeTypes.hpp"

#include "http/HttpStatus.hpp"

HttpResponse RedirectHandler::handle(const RedirectConfig& redirect, const ResolvedRoute& route) {
    if (Http::Status::isRedirect(redirect.status)) {
        return HttpResponseFactory::create(redirect.status, HttpHeaders{{Http::Headers::Location, *redirect.target}});
    }

    if (redirect.target) {
        return HttpResponseFactory::create(redirect.status, *redirect.target, std::string(Http::Mime::Text));
    }

    if (Http::Status::isError(redirect.status)) {
        return ErrorResponseFactory::create(redirect.status, route);
    }

    return HttpResponseFactory::create(redirect.status);
}
