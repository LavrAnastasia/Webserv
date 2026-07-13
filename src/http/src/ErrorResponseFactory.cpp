#include "ErrorResponseFactory.hpp"

#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include "HttpResponseFactory.hpp"
#include "HttpStatusUtils.hpp"

namespace {
    std::string buildHtml(HttpStatus status) {
        const int statusCode = static_cast<int>(status);
        const std::string reason = Http::Status::toString(status);

        return "<!DOCTYPE html>\n"
               "<html lang=\"en\">\n"
               "<head>\n"
               "    <meta charset=\"UTF-8\">\n"
               "    <title>" +
            std::to_string(statusCode) + " " + reason +
            "</title>\n"
            "</head>\n"
            "<body>\n"
            "    <h1>" +
            std::to_string(statusCode) + " " + reason +
            "</h1>\n"
            "</body>\n"
            "</html>\n";
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
} // namespace

HttpResponse ErrorResponseFactory::create(HttpStatus status) {
    return HttpResponseFactory::create(
        status,
        buildHtml(status),
        "text/html" // TODO: replace with HttpContentType::Html
    );
}

HttpResponse ErrorResponseFactory::create(HttpStatus status, const ResolvedRoute& route) {
    const auto it = route.errorPages.find(static_cast<int>(status));

    if (it == route.errorPages.end()) {
        return create(status);
    }

    std::optional<std::string> body = readBody(it->second);

    if (!body.has_value()) {
        return create(status);
    }

    return HttpResponseFactory::create(status, std::move(*body), "text/html");
}
