#include "UploadHandler.hpp"

#include "ErrorResponseFactory.hpp"
#include "ErrorStatus.hpp"
#include "HeaderFields.hpp"
#include "HttpResponseFactory.hpp"
#include "UrlCodec.hpp"
#include "fs/Path.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

HttpResponse UploadHandler::handle(const HttpRequest& request, const ResolvedRoute& route) {
    try {
        // build configured upload path relative to route root
        const fs::path uploadPath = Fs::resolve(route.root, route.upload->uploadPath);

        // configured upload path must exist and be a directory
        if (!fs::is_directory(uploadPath)) {
            return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
        }

        // canonicalize validated upload directory
        const fs::path uploadRoot = fs::canonical(uploadPath);

        // extract request-relative upload path
        const fs::path relativePath = request.path.substr(route.locationPath.size());

        if (relativePath.empty() || relativePath == "/") {
            return ErrorResponseFactory::create(HttpStatus::Conflict, route);
        }

        // build filesystem target in configured upload root, then normalize it
        const fs::path target = fs::weakly_canonical(Fs::resolve(uploadRoot, relativePath));

        // target escapes upload root -> 403 forbidden
        if (!Fs::isPrefixOf(uploadRoot, target)) {
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
        return HttpResponseFactory::create(
            HttpStatus::Created, HttpHeaders{{Http::Headers::Location, Http::Url::encodePath(request.path)}}
        );
    } catch (const fs::filesystem_error& error) {
        // handle failures from throwing std::filesystem operations
        // and map underlying error to corresponding HTTP status
        return ErrorResponseFactory::create(Http::Status::from(error.code()), route);
    }
}
