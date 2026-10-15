#pragma once

#include <memory>
#include <variant>

#include "RequestHandler.hpp"
#include "ResolvedRoute.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

class StaticHandler : public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    static DispatchResult start(HttpRequest request, ResolvedRoute route);
    HandlerResult handle() override;
};
