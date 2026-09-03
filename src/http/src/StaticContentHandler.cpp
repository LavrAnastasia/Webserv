#include "StaticContentHandler.hpp"

#include "HttpStatusUtils.hpp"

#include "ErrorResponseFactory.hpp"
#include "RegularResponseFactory.hpp"

#include "fs/PathUtils.hpp"

#include "AutoindexResponseFactory.hpp"
#include "HttpHeadersUtils.hpp"
#include "HttpSyntax.hpp"
#include "UrlCodec.hpp"

namespace {
    namespace fs = std::filesystem;

    HttpResponse createRedirectResponse(const HttpRequest& request) {
        std::string location = Http::Url::encodePath(request.path);

        location += Http::Syntax::PathPrefix;

        if (!request.query.empty()) {
            location += Http::Syntax::QuerySeparator;
            location += request.query;
        }

        HttpResponse response{};

        response.status = HttpStatus::MovedPermanently;
        response.headers.set(std::string(Http::Headers::Location), location);
        response.headers.set(std::string(Http::Headers::ContentLength), "0");
        return response;
    }

    HttpResponse serveDirectory(
        const fs::path& directoryPath, const fs::path& root, const HttpRequest& request, const ResolvedRoute& route
    ) {
        if (request.path.empty()) {
            return ErrorResponseFactory::create(HttpStatus::BadRequest, route);
        }

        if (request.path.back() != Http::Syntax::PathPrefix) {
            return createRedirectResponse(request);
        }

        if (!route.index.empty()) {
            std::error_code error;

            const fs::path indexPath = fs::weakly_canonical(directoryPath / route.index, error);

            if (error) {
                const HttpStatus status = Http::Status::from(error);
                if (status != HttpStatus::NotFound) {
                    return ErrorResponseFactory::create(status, route);
                }
            } else {
                if (!Fs::isPrefixOf(root, indexPath)) {
                    return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
                }

                const fs::file_status indexStatus = fs::status(indexPath, error);

                if (error) {
                    const HttpStatus status = Http::Status::from(error);
                    if (status != HttpStatus::NotFound) {
                        return ErrorResponseFactory::create(status, route);
                    }
                } else if (fs::is_regular_file(indexStatus)) {
                    return RegularResponseFactory::create(indexPath, route);
                } else if (fs::exists(indexStatus)) {
                    return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
                }
            }
        }

        if (route.autoindex) {
            return AutoindexResponseFactory::create(directoryPath, request, route);
        }
        return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
    }
} // namespace

HttpResponse StaticContentHandler::handle(const HttpRequest& request, const ResolvedRoute& route) {
    if (request.path.empty() || request.path.front() != '/') {
        return ErrorResponseFactory::create(HttpStatus::BadRequest, route);
    }

    std::error_code error;

    const fs::path root = fs::weakly_canonical(route.root, error);

    if (error) {
        return ErrorResponseFactory::create(Http::Status::from(error), route);
    }

    const fs::path relativePath = fs::path(request.path).relative_path();

    const fs::path filePath = fs::weakly_canonical(root / relativePath, error);

    if (error) {
        return ErrorResponseFactory::create(Http::Status::from(error), route);
    }

    if (!Fs::isPrefixOf(root, filePath)) {
        return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
    }

    const fs::file_status fileStatus = fs::status(filePath, error);

    if (error) {
        return ErrorResponseFactory::create(Http::Status::from(error), route);
    }

    if (!fs::exists(fileStatus)) {
        return ErrorResponseFactory::create(HttpStatus::NotFound, route);
    }

    if (fs::is_directory(fileStatus)) {
        return serveDirectory(filePath, root, request, route);
    }

    if (!fs::is_regular_file(fileStatus)) {
        return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
    }
    return RegularResponseFactory::create(filePath, route);
}
