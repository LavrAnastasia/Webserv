#include "CgiHandler.hpp"
#include "CgiEnvironment.hpp"
#include "ErrorResponseFactory.hpp"
#include "ErrorStatus.hpp"

#include "fs/Path.hpp"

namespace fs = std::filesystem;

HandlerResult CgiHandler::handle(const HttpRequest& request, const ResolvedRoute& route) {
    try {
        const fs::path root = fs::weakly_canonical(route.root);
        const fs::path script = fs::weakly_canonical(Fs::resolve(root, request.path));

        if (!Fs::isPrefixOf(root, script)) {
            return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
        }

        const fs::file_status status = fs::status(script);

        if (!fs::exists(status)) {
            return ErrorResponseFactory::create(HttpStatus::NotFound, route);
        }

        if (!fs::is_regular_file(status)) {
            return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
        }

        return CgiRequest{
            .interpreter = route.cgi->interpreter,
            .script = script,
            .env = CgiEnvironment::build(request, script),
            .body = request.body
        };
    } catch (const fs::filesystem_error& error) {
        return ErrorResponseFactory::create(Http::Status::from(error.code()), route);
    }
}
