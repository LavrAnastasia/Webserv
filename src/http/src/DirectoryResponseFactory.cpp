#include "DirectoryResponseFactory.hpp"


#include "HttpUtils.hpp"

#include "AutoindexResponseFactory.hpp"
#include "ErrorResponseFactory.hpp"
#include "HttpSyntax.hpp"
#include "RegularResponseFactory.hpp"

namespace {
    namespace fs = std::filesystem;

    bool isUrlUnreserved(unsigned char character) {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' || character == '.' || character == '_' ||
            character == '~';
    }

    std::string encodeUrlPath(const std::string& path) {
        constexpr char hex[] = "0123456789ABCDEF";

        std::string result;

        for (const unsigned char character : path) {
            if (character == '/' || isUrlUnreserved(character)) {
                result += static_cast<char>(character);
                continue;
            }

            result += '%';
            result += hex[character >> 4];
            result += hex[character & 0x0F];
        }
        return result;
    }

    HttpResponse createRedirectResponse(const HttpRequest& request) {
        std::string location = encodeUrlPath(request.path);

        location += Http::Syntax::PathPrefix;

        if (!request.query.empty()) {
            location += Http::Syntax::QuerySeparator;
            location += request.query;
        }

        HttpResponse response{};

        response.status = HttpStatus::MovedPermanently;
        response.headers.set("Location", location);
        response.headers.set("Content-Length", "0");

        return response;
    }
} // namespace

HttpResponse DirectoryResponseFactory::create(
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
            if (!Http::isNotFoundError(error)) {
                return ErrorResponseFactory::create(Http::statusFromError(error), route);
            }
        } else {
            if (!Http::isWithinRoot(indexPath, root)) {
                return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
            }

            const fs::file_status indexStatus = fs::status(indexPath, error);

            if (error) {
                if (!Http::isNotFoundError(error)) {
                    return ErrorResponseFactory::create(Http::statusFromError(error), route);
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
