#pragma once

#include <charconv>
#include <utility>

#include <gtest/gtest.h>

#include "../../src/http/src/HeaderFields.hpp"
#include "../../src/http/src/HeadersParser.hpp"
#include "../../src/http/src/HttpSyntax.hpp"
#include "../../src/http/src/RequestDispatcher.hpp"
#include "../../src/http/src/UrlCodec.hpp"
#include "http/HttpSession.hpp"

inline HandlerResult dispatch(HttpRequest request, const ServerConfig& server, const ConnectionInfo& info) {
    request.headers.set(std::string(Http::Headers::ContentLength), std::to_string(request.body.size()));
    std::string target = Http::Url::encodePath(request.path);
    if (!request.query.empty()) {
        target += Http::Syntax::QuerySeparator + request.query;
    }
    const std::string wire = Http::Method::toString(request.method) + Http::Syntax::SP + target + Http::Syntax::SP +
        request.version + std::string(Http::Syntax::CRLF) + request.headers.serialize() +
        std::string(Http::Syntax::CRLF) + request.body;

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

    const auto line = bytes.find(Http::Syntax::CRLF);
    const auto end = bytes.find(Http::Syntax::HeaderSectionEnd);
    const auto space = bytes.find(Http::Syntax::SP);
    if (line == std::string::npos || end == std::string::npos || space >= line) {
        ADD_FAILURE() << "Session did not produce an HTTP response";
        return HttpResponse{HttpStatus::InternalServerError};
    }
    int code{};
    const auto parsed = std::from_chars(bytes.data() + space + 1, bytes.data() + line, code);
    const auto status = Http::Status::fromCode(code);
    auto headers = HeadersParser::parse(bytes.substr(line + Http::Syntax::CRLF.size(), end - line));
    if (parsed.ec != std::errc{} || !status || !headers) {
        ADD_FAILURE() << "Session produced invalid response headers";
        return HttpResponse{HttpStatus::InternalServerError};
    }
    return HttpResponse{
        *status, std::move(*headers), ResponseBody(bytes.substr(end + Http::Syntax::HeaderSectionEnd.size()))
    };
}
