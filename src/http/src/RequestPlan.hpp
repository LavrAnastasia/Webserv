#pragma once

#include <memory>
#include <string>
#include <variant>

#include "ResolvedRoute.hpp"
#include "UploadHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

struct RequestPlan {
    struct Serve {};
    struct Invoke {};
    struct Store {};
    using Handler = std::variant<HttpResponse, Serve, Invoke, Store>;
    using Body = std::variant<std::monostate, std::string, std::unique_ptr<UploadHandler>>;

    HttpRequest request;
    ResolvedRoute route;
    Handler handler;
    Body body;
    bool expectContinue = false;
};
