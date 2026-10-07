#include "ErrorResponseFactory.hpp"

#include <string>
#include <utility>

#include "http/HttpMethod.hpp"
#include "http/HttpStatus.hpp"

#include "HeaderFields.hpp"
#include "HttpHtmlUtils.hpp"
#include "HttpResponseFactory.hpp"
#include "MimeTypes.hpp"

namespace {
    std::string buildHtml(HttpStatus status) {
        const int statusCode = static_cast<int>(status);
        const std::string reason = Http::Status::toString(status);

        const std::string title = std::to_string(statusCode) + " " + reason;

        return Http::Html::buildPage(title, title);
    }

    HttpResponse buildResponse(HttpStatus status) {
        return HttpResponseFactory::create(status, buildHtml(status), std::string(Http::Mime::Html));
    }

    HttpResponse buildResponse(HttpStatus status, const ErrorPages& errorPages) {
        const auto it = errorPages.find(status);

        if (it == errorPages.end()) {
            return buildResponse(status);
        }

        auto body = ResponseBody::open(it->second);

        if (std::holds_alternative<std::error_code>(body)) {
            return buildResponse(status);
        }

        return HttpResponse{
            .status = status,
            .headers = HttpHeaders{{Http::Headers::ContentType, Http::Mime::Html}},
            .body = std::move(std::get<ResponseBody>(body))
        };
    }
} // namespace

HttpResponse ErrorResponseFactory::create(HttpStatus status, const ResolvedRoute& route) {
    HttpResponse response = buildResponse(status, route.errorPages);

    if (status == HttpStatus::MethodNotAllowed) {
        response.headers.set(std::string(Http::Headers::Allow), Http::Method::toString(route.allowedMethods));
    }

    return response;
}

HttpResponse ErrorResponseFactory::create(HttpStatus status, const ErrorPages& pages) {
    return buildResponse(status, pages);
}
