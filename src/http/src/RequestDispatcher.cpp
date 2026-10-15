#include "http/RequestDispatcher.hpp"

#include <limits>
#include <utility>

#include "CgiHandler.hpp"
#include "ErrorResponseFactory.hpp"
#include "HeaderFields.hpp"
#include "RedirectHandler.hpp"
#include "RequestPlan.hpp"
#include "Router.hpp"
#include "StaticHandler.hpp"
#include "UploadHandler.hpp"

namespace {
    constexpr std::size_t kUnlimitedBodySize = std::numeric_limits<std::size_t>::max();
} // namespace

std::unique_ptr<RequestPlan> RequestDispatcher::select(HttpRequest request, const ServerConfig& server) {
    auto route = Router::resolve(request, server);
    auto plan = std::make_unique<RequestPlan>();
    plan->request = std::move(request);
    if (route) {
        plan->route = std::move(*route);
    } else {
        plan->route.clientMaxBodySize = kUnlimitedBodySize;
        plan->route.errorPages = Router::mapErrorPages(server);
    }

    const auto expect = plan->request.headers.get(Http::Headers::Expect);
    if (expect && !HttpHeaders::equals(*expect, Http::Headers::Expectation::Continue)) {
        plan->handler = ErrorResponseFactory::create(HttpStatus::ExpectationFailed, plan->route);
        return plan;
    }
    plan->expectContinue = expect.has_value();

    if (!route) {
        plan->handler = ErrorResponseFactory::create(HttpStatus::NotFound, plan->route);
    } else if (plan->route.redirect) {
        plan->handler = RedirectHandler::handle(*plan->route.redirect, plan->route);
    } else if (!plan->route.allowedMethods.contains(plan->request.method)) {
        plan->handler = ErrorResponseFactory::create(HttpStatus::MethodNotAllowed, plan->route);
    } else if (plan->route.cgi) {
        plan->handler = RequestPlan::Invoke{};
        plan->body = std::string{};
    } else if (plan->route.upload && plan->request.method == HttpMethod::Post) {
        plan->handler = RequestPlan::Store{};
        plan->body = std::make_unique<UploadHandler>();
    } else {
        plan->handler = RequestPlan::Serve{};
    }
    return plan;
}

HandlerResult RequestDispatcher::dispatch(RequestPlan plan, const ConnectionInfo& connectionInfo) {
    if (auto* bytes = std::get_if<std::string>(&plan.body)) {
        plan.request.body = std::move(*bytes);
    }
    if (auto* response = std::get_if<HttpResponse>(&plan.handler)) {
        return std::move(*response);
    }
    if (std::holds_alternative<RequestPlan::Invoke>(plan.handler)) {
        return CgiHandler::handle(std::move(plan.request), plan.route, connectionInfo);
    }
    if (std::holds_alternative<RequestPlan::Store>(plan.handler)) {
        return std::get<std::unique_ptr<UploadHandler>>(plan.body)->end();
    }
    return StaticHandler::handle(plan.request, plan.route);
}
