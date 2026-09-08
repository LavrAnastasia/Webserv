#include "ErrorResponseFactory.hpp"

#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include "http/HttpMethodUtils.hpp"

#include "HttpHeadersUtils.hpp"
#include "HttpHtmlUtils.hpp"
#include "HttpResponseFactory.hpp"
#include "HttpStatusUtils.hpp"
#include "MimeTypes.hpp"

namespace {
    std::string buildHtml(HttpStatus status) {
        const int statusCode = static_cast<int>(status);
        const std::string reason = Http::Status::toString(status);

        const std::string title = std::to_string(statusCode) + " " + reason;

        return Http::Html::buildPage(title, title);
    }

    std::optional<std::string> readBody(const std::filesystem::path& path) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) {
            return std::nullopt;
        }

        std::ifstream file(path, std::ios::binary);

        if (!file.is_open()) {
            return std::nullopt;
        }

        std::string body{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

        if (file.bad()) {
            return std::nullopt;
        }

        return body;
    }

    HttpResponse buildResponse(HttpStatus status, const ResolvedRoute& route) {
        const auto it = route.errorPages.find(static_cast<int>(status));

        if (it == route.errorPages.end()) {
            return ErrorResponseFactory::create(status);
        }

        std::optional<std::string> body = readBody(it->second);

        if (!body.has_value()) {
            return ErrorResponseFactory::create(status);
        }

        return HttpResponseFactory::create(status, std::move(*body), std::string(Http::Mime::Html));
    }
} // namespace

HttpResponse ErrorResponseFactory::create(HttpStatus status) {
    return HttpResponseFactory::create(status, buildHtml(status), std::string(Http::Mime::Html));
}

HttpResponse ErrorResponseFactory::create(HttpStatus status, const ResolvedRoute& route) {
    HttpResponse response = buildResponse(status, route);

    if (status == HttpStatus::MethodNotAllowed) {
        response.headers.set(std::string(Http::Headers::Allow), Http::Method::toString(route.allowedMethods));
    }

    return response;
}
