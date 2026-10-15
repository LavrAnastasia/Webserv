#include "CgiHandler.hpp"

#include <algorithm>
#include <utility>

#include "CgiEnvironment.hpp"
#include "ErrorResponseFactory.hpp"
#include "ErrorStatus.hpp"
#include "fs/Path.hpp"

namespace fs = std::filesystem;

namespace {
    constexpr std::size_t kBodyReserve = 128u * 1024u * 1024u; // 128 MB
} // namespace

CgiHandler::CgiHandler(HttpRequest request, ResolvedRoute route, ConnectionInfo info)
    : RequestHandler(std::move(request), std::move(route)), info_(std::move(info)) {
}

DispatchResult
CgiHandler::start(HttpRequest request, ResolvedRoute route, ConnectionInfo info, std::optional<std::size_t> length) {
    if (length) {
        request.body.reserve(std::min(*length, kBodyReserve));
    }

    return std::make_unique<CgiHandler>(std::move(request), std::move(route), std::move(info));
}

bool CgiHandler::receive(std::string_view body) {
    request_.body.append(body);
    return true;
}

HandlerResult CgiHandler::handle() {
    try {
        const fs::path root = fs::weakly_canonical(route_.root);
        const fs::path script = fs::weakly_canonical(Fs::resolve(root, request_.path));

        if (!Fs::isPrefixOf(root, script)) {
            return ErrorResponseFactory::create(HttpStatus::Forbidden, route_);
        }

        const fs::file_status status = fs::status(script);

        if (fs::exists(status) && !fs::is_regular_file(status)) {
            return ErrorResponseFactory::create(HttpStatus::Forbidden, route_);
        }

        return CgiRequest{
            .interpreter = route_.cgi->interpreter,
            .script = script,
            .env = CgiEnvironment::build(request_, script, info_),
            .body = std::move(request_.body)
        };
    } catch (const fs::filesystem_error& error) {
        return ErrorResponseFactory::create(Http::Status::from(error.code()), route_);
    }
}
