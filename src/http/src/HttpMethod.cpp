#include "http/HttpMethod.hpp"

std::optional<HttpMethod> parseHttpMethod(const std::string& value) {
    if (value == "GET")
        return HttpMethod::Get;
    if (value == "POST")
        return HttpMethod::Post;
    if (value == "DELETE")
        return HttpMethod::Delete;
    return std::nullopt;
}

std::string toString(HttpMethod method) {
    switch (method) {
        case HttpMethod::Get:
            return "GET";
        case HttpMethod::Post:
            return "POST";
        case HttpMethod::Delete:
            return "DELETE";
    }
    return "";
}
