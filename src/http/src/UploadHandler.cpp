#include "UploadHandler.hpp"
#include "ErrorResponseFactory.hpp"
#include "fs/Path.hpp"

#include <filesystem>
#include <fstream>
#include <optional>

namespace fs = std::filesystem;

namespace {

    std::optional<fs::path> resolveTarget(
        const HttpRequest& request, const ResolvedRoute& route, const fs::path& uploadRoot, std::error_code& error
    ) {
        // 1.   Remove matched upload location from request path:
        //      request.path: /uploads/images/cat.png +
        //      route.locationPath: /uploads =
        //      /images/cat.png
        const fs::path relativePath = request.path.substr(route.locationPath.size());

        //  Client did not specify a file inside the upload location
        if (relativePath.empty() || relativePath == "/") {
            return std::nullopt;
        }

        // 2.   Build filesystem target in configured upload root, then normalize it
        //      resolve() <- uploadRoot: /project/public/uploads +
        //      relativePath: /images/cat.png =
        //      /project/public/uploads/images/cat.png
        const fs::path target = fs::weakly_canonical(Fs::resolve(uploadRoot, relativePath), error);

        //  Return nullopt if path resolution failed, or normalized target outside uploadRoot
        //  NOTE: error modified only if path resolution failed, otherwise empty
        if (error || !Fs::isPrefixOf(uploadRoot, target)) {
            return std::nullopt;
        }

        return target;
    }

} //namespace

HttpResponse UploadHandler::handle(const HttpRequest& request, const ResolvedRoute& route) {
    // validate configured upload root directory:
    // must exist, resolve successfully and be a directory, ELSE -> error 500
    std::error_code error;
    const fs::path uploadRoot = fs::canonical(route.upload->uploadPath, error);
    if (error || !fs::is_directory(uploadRoot, error)) {
        return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
    }

    // validate client path:
    const auto target = resolveTarget(request, route, uploadRoot, error);
    // path resolution failed & error set -> error 500
    // error not set (target outside upload directory)-> error 403
    if (!target) {
        if (error) {
            return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
        }
        return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
    }

    // validate that the target doesn't already exist, ELSE -> error 409
    error.clear();
    if (fs::exists(*target, error)) {
        return ErrorResponseFactory::create(HttpStatus::Conflict, route);
    }

    // could not verify (filesystem operation failed) -> error 500
    if (error) {
        return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
    }

    // validate parent directory:
    // must already exist and be a directory, ELSE -> error 500
    // could not verify (filesystem operation failed) -> also error 500
    error.clear();
    if (!fs::is_directory(target->parent_path(), error) || error) {
        return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
    }

    // open fs path stored inside (optional) target for writing, treat file as raw binary data
    std::ofstream file(*target, std::ios::binary);

    // failed to open/create destination file -> error 500
    if (!file) {
        return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
    }

    file.write(request.body.data(), static_cast<std::streamsize>(request.body.size()));
    file.close();

    // write or close failure puts stream into failed state -> error 500
    if (!file) {
        std::error_code cleanupError; // stores remove() failure error code to avoid thrown exception
        fs::remove(*target, cleanupError);

        return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
    }

    // upload completed successfully
    HttpResponse response{};
    response.status = HttpStatus::Created;
    return response;
}
