#include "http/HttpRequest.hpp"

namespace {
    struct RequestLineParts {
        std::string method_text;
        std::string target;
        std::string version;
    };

    bool isValidHttpVersion(const std::string& version) {
        return version == "HTTP/1.1" || version == "HTTP/1.0";
    }

    bool isValidRequestTarget(const std::string& target) {
        return !target.empty() && target[0] == '/';
    }

    void fillPathAndQuery(HttpRequest& request) {
        std::size_t is_query;
        is_query = request.target.find('?');
        if (is_query == std::string::npos) {
            request.path = request.target;
            request.query = "";
        } else {
            request.path = request.target.substr(0, is_query);
            request.query = request.target.substr(is_query + 1);
        }
    }

    std::optional<RequestLineParts> splitRequestLine(const std::string& line) {
        std::size_t first_space;
        std::size_t second_space;
        std::size_t third_space;

        first_space = line.find(' ');
        if (first_space == std::string::npos)
            return std::nullopt;
        second_space = line.find(' ', first_space + 1);
        if (second_space == std::string::npos)
            return std::nullopt;
        third_space = line.find(' ', second_space + 1);
        if (third_space != std::string::npos)
            return std::nullopt;

        RequestLineParts parts;
        parts.method_text = line.substr(0, first_space);
        parts.target = line.substr(first_space + 1, second_space - first_space - 1);
        parts.version = line.substr(second_space + 1);

        if (parts.method_text.empty() || parts.target.empty() || parts.version.empty())
            return std::nullopt;

        return parts;
    }
} // namespace

std::optional<HttpRequest> parseRequestLine(const std::string& line) {
    std::optional<RequestLineParts> parts = splitRequestLine(line);
    if (!parts)
        return std::nullopt;

    std::optional<HttpMethod> method = parseHttpMethod(parts->method_text);
    if (!method)
        return std::nullopt;
    if (!isValidRequestTarget(parts->target))
        return std::nullopt;
    if (!isValidHttpVersion(parts->version))
        return std::nullopt;
    HttpRequest request;
    request.method = *method;
    request.target = parts->target;
    request.version = parts->version;
    request.body = "";

    fillPathAndQuery(request);
    return request;
}
