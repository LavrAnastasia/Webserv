#pragma once

#include <cstddef>
#include <string_view>

#include "RequestDispatcher.hpp"
#include "ResolvedRoute.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpStatus.hpp"

class RequestHandler {
public:
    RequestHandler(HttpRequest request, ResolvedRoute route);
    virtual ~RequestHandler();
    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    virtual bool receive(std::string_view body);
    virtual HandlerResult handle() = 0;

    std::size_t limit() const;
    HttpResponse reject(HttpStatus status) const;

protected:
    HttpRequest request_;
    ResolvedRoute route_;
};
