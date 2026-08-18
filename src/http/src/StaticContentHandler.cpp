#include "StaticContentHandler.hpp"

#include "HttpUtils.hpp"

#include "DirectoryResponseFactory.hpp"
#include "ErrorResponseFactory.hpp"
#include "RegularResponseFactory.hpp"

namespace {
    namespace fs = std::filesystem;
} // namespace

HttpResponse StaticContentHandler::handle(const HttpRequest& request, const ResolvedRoute& route) {
    if (request.path.empty() || request.path.front() != '/') {
        return ErrorResponseFactory::create(HttpStatus::BadRequest, route);
    }

    std::error_code error;

    const fs::path root = fs::weakly_canonical(route.root, error);

    if (error) {
        return ErrorResponseFactory::create(Http::statusFromError(error), route);
    }

    const fs::path relativePath = fs::path(request.path).relative_path();

    const fs::path filePath = fs::weakly_canonical(root / relativePath, error);

    if (error) {
        return ErrorResponseFactory::create(Http::statusFromError(error), route);
    }

    if (!Http::isWithinRoot(filePath, root)) {
        return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
    }

    const fs::file_status fileStatus = fs::status(filePath, error);

    if (error) {
        return ErrorResponseFactory::create(Http::statusFromError(error), route);
    }

    if (!fs::exists(fileStatus)) {
        return ErrorResponseFactory::create(HttpStatus::NotFound, route);
    }

    if (fs::is_directory(fileStatus)) {
        return DirectoryResponseFactory::create(filePath, root, request, route);
    }

    if (!fs::is_regular_file(fileStatus)) {
        return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
    }

    return RegularResponseFactory::create(filePath, route);
}
