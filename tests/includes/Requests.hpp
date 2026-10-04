#pragma once

#include <string>
#include <string_view>

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

    inline std::string rawGet(std::string_view path, std::string_view connection = {}) {
        std::string request = "GET ";
        request.append(path);
        request += " HTTP/1.1\r\nHost: localhost\r\n";

        if (!connection.empty()) {
            request += "Connection: ";
            request.append(connection);
            request += "\r\n";
        }

        request += "\r\n";
        return request;
    }

}
