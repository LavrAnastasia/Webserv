#include "RedirectHandler.hpp"
#include "HttpHeadersUtils.hpp"

HttpResponse RedirectHandler::handle(const RedirectConfig& redirect) {
    HttpResponse response{};

    response.status = static_cast<HttpStatus>(redirect.statusCode);
    response.headers.set(std::string(Http::Headers::Location), redirect.target);

    return response;
}
