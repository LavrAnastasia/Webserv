#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <variant>

#include "RequestHandler.hpp"
#include "ResolvedRoute.hpp"
#include "http/ConnectionInfo.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

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
