#include "UploadHandler.hpp"

#include "ErrorResponseFactory.hpp"
#include "HttpStatusUtils.hpp"
#include "fs/Path.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

    fs::path resolveTarget(const fs::path& relativePath, const fs::path& uploadRoot) {
        //      Build filesystem target in configured upload root, then normalize it
        //      uploadRoot:     /project/public/uploads
        //      relativePath:   /images/cat.png
        //      target:         /project/public/uploads/images/cat.png
        const fs::path target = fs::weakly_canonical(Fs::resolve(uploadRoot, relativePath));

        // reject normalized targets outside of configured upload root
        if (!Fs::isPrefixOf(uploadRoot, target)) {
            return {};
        }

        return target;
    }

} // namespace

HttpResponse UploadHandler::handle(const HttpRequest& request, const ResolvedRoute& route) {
    try {
        // resolve configured upload path relative to the route root
        const fs::path uploadRoot = fs::canonical(Fs::resolve(route.root, route.upload->uploadPath));

        if (!fs::is_directory(uploadRoot)) {
            return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
        }

        // extract request-relative upload path
        const fs::path relativePath = request.path.substr(route.locationPath.size());

        // no file name specified in upload location -> 400 bad request
        if (relativePath.empty() || relativePath == "/") {
            return ErrorResponseFactory::create(HttpStatus::BadRequest, route);
        }

        // validate client upload target
        const fs::path target = resolveTarget(relativePath, uploadRoot);

        // target escapes upload root -> 403 forbidden
        if (target.empty()) {
            return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
        }

        // target already exists -> 409 conflict
        if (fs::exists(target)) {
            return ErrorResponseFactory::create(HttpStatus::Conflict, route);
        }

        // parent path isn't a directory -> 500 internal server error
        if (!fs::is_directory(target.parent_path())) {
            return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
        }

        // open target for binary output
        std::ofstream file(target, std::ios::binary);

        // failed to open/create destination file -> error 500
        if (!file) {
            return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
        }

        file.write(request.body.data(), static_cast<std::streamsize>(request.body.size()));
        file.close();

        // write or close failure puts stream into failed state -> error 500
        if (!file) {
            // stores remove() failure error code to avoid thrown exception
            std::error_code cleanupError;
            // best-effort cleanup: use error_code overload so remove can't throw
            fs::remove(target, cleanupError);

            return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
        }

        // upload completed successfully
        HttpResponse response{};
        response.status = HttpStatus::Created;
        return response;

    } catch (const fs::filesystem_error& error) {
        // handle failures from throwing std::filesystem operations
        // and map underlying error to corresponding HTTP status
        return ErrorResponseFactory::create(Http::Status::from(error.code()), route);
    }
}
