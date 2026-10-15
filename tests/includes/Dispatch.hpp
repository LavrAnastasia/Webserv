#pragma once

#include <cctype>
#include <charconv>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

#include "http/HttpSession.hpp"
#include "http/RequestDispatcher.hpp"

inline std::string encodeTarget(std::string_view path) {
    std::string encoded;

    for (const unsigned char character : path) {
        if (std::isalnum(character) || std::string_view("/-._~").find(character) != std::string_view::npos) {
            encoded += static_cast<char>(character);
        } else {
            char escaped[4];
            std::snprintf(escaped, sizeof(escaped), "%%%02X", character);
            encoded += escaped;
        }
    }

    return encoded;
}

inline HttpHeaders parseHeaders(std::string_view block) {
    HttpHeaders headers;

    while (!block.empty()) {
        const auto lineEnd = block.find("\r\n");
        const std::string_view line = block.substr(0, lineEnd);
        const auto colon = line.find(':');

        if (colon != std::string_view::npos) {
            std::string_view value = line.substr(colon + 1);
            while (!value.empty() && value.front() == ' ') {
                value.remove_prefix(1);
            }
            headers.add(std::string(line.substr(0, colon)), std::string(value));
        }

        if (lineEnd == std::string_view::npos) {
            break;
        }
        block.remove_prefix(lineEnd + 2);
    }

    return headers;
}

inline HandlerResult dispatch(HttpRequest request, const ServerConfig& server, const ConnectionInfo& info) {
    request.headers.set("Content-Length", std::to_string(request.body.size()));
    std::string target = encodeTarget(request.path);
    if (!request.query.empty()) {
        target += "?" + request.query;
    }
    const std::string wire = Http::Method::toString(request.method) + " " + target + " " + request.version + "\r\n" +
        request.headers.serialize() + "\r\n" + request.body;

    HttpSession session(server, info);
    auto action = session.receive(wire);
    if (auto* cgi = std::get_if<CgiRequest>(&action)) {
        return std::move(*cgi);
    }

    std::string bytes;
    while (auto* write = std::get_if<HttpSession::Write>(&action)) {
        bytes += write->bytes;
        action = session.emit();
    }

    const auto line = bytes.find("\r\n");
    const auto end = bytes.find("\r\n\r\n");
    const auto space = bytes.find(' ');
    if (line == std::string::npos || end == std::string::npos || space >= line) {
        ADD_FAILURE() << "Session did not produce an HTTP response";
        return HttpResponse{HttpStatus::InternalServerError};
    }
    int code{};
    const auto parsed = std::from_chars(bytes.data() + space + 1, bytes.data() + line, code);
    const auto status = Http::Status::fromCode(code);
    if (parsed.ec != std::errc{} || !status) {
        ADD_FAILURE() << "Session produced an invalid status line";
        return HttpResponse{HttpStatus::InternalServerError};
    }
    return HttpResponse{
        *status,
        parseHeaders(std::string_view(bytes).substr(line + 2, end - line - 2)),
        ResponseBody(bytes.substr(end + 4))
    };
}
