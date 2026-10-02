#pragma once

#include <string>

#include "http/HttpRequest.hpp"

namespace Requests {

    inline HttpRequest make(HttpMethod method, const std::string& path, const std::string& body = "") {
        HttpRequest request{};
        request.method = method;
        request.target = path;
        request.path = path;
        request.version = "HTTP/1.1";
        request.headers.set("Host", "localhost");
        request.body = body;
        return request;
    }

    inline HttpRequest get(const std::string& path, const std::string& body = "") {
        return make(HttpMethod::Get, path, body);
    }

    inline HttpRequest post(const std::string& path, const std::string& body = "") {
        return make(HttpMethod::Post, path, body);
    }

} // namespace Requests
