#include "CgiHandler.hpp"

#include <utility>

#include "CgiEnvironment.hpp"
#include "ErrorResponseFactory.hpp"
#include "ErrorStatus.hpp"
#include "fs/Path.hpp"

namespace fs = std::filesystem;

HandlerResult
CgiHandler::handle(HttpRequest request, const ResolvedRoute& route, const ConnectionInfo& connectionInfo) {
    std::error_code error;
    const fs::path root = fs::weakly_canonical(route.root, error);
    if (error) {
        return ErrorResponseFactory::create(Http::Status::from(error), route);
    }
    const fs::path script = fs::weakly_canonical(Fs::resolve(root, request.path), error);
    if (error) {
        return ErrorResponseFactory::create(Http::Status::from(error), route);
    }
    if (!Fs::isPrefixOf(root, script)) {
        return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
    }

    const fs::file_status status = fs::status(script, error);
    if (error && error != std::errc::no_such_file_or_directory) {
        return ErrorResponseFactory::create(Http::Status::from(error), route);
    }
    if (fs::exists(status) && !fs::is_regular_file(status)) {
        return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
    }

    return CgiRequest{
        .interpreter = route.cgi->interpreter,
        .script = script,
        .env = CgiEnvironment::build(request, script, connectionInfo),
        .body = std::move(request.body)
    };
}
