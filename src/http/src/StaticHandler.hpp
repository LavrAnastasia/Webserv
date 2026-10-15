#pragma once


#include "RequestHandler.hpp"
#include "ResolvedRoute.hpp"
#include "http/HttpRequest.hpp"

class StaticHandler : public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    static DispatchResult start(HttpRequest request, ResolvedRoute route);
    HandlerResult handle() override;
};
