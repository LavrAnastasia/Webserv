#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

#include "RequestHandler.hpp"
#include "ResolvedRoute.hpp"
#include "http/ConnectionInfo.hpp"
#include "http/HttpRequest.hpp"

class CgiHandler : public RequestHandler {
public:
    CgiHandler(HttpRequest request, ResolvedRoute route, ConnectionInfo info);

    static DispatchResult
    start(HttpRequest request, ResolvedRoute route, ConnectionInfo info, std::optional<std::size_t> length);
    bool receive(std::string_view body) override;
    HandlerResult handle() override;

private:
    ConnectionInfo info_;
};
