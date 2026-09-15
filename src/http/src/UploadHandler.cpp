#include "UploadHandler.hpp"
#include "ErrorResponseFactory.hpp"
#include "fs/Path.hpp"

#include <filesystem>
#include <fstream>
#include <optional>

namespace fs = std::filesystem;

/*
!   FAIL CAUSES
    bad client path
    → target escapes upload root
    → 403 Forbidden

    bad server/config state
    → upload_path missing / not a directory / canonical() fails
    → 500 Internal Server Error
*/

namespace {

    std::optional<fs::path>
    resolveTarget(const HttpRequest& request, const ResolvedRoute& route, const fs::path& uploadRoot) {
        // 1.   Remove matched upload location from request path:
        //      request.path: /uploads/images/cat.png +
        //      route.locationPath: /uploads =
        //      /images/cat.png
        const fs::path relativePath = request.path.substr(route.locationPath.size());

        // 2.   Verify that the request identifies a file inside the upload location
        if (relativePath.empty() || relativePath == "/") {
            return std::nullopt;
        }

        // 3.   Build filesystem target in configured upload root, then normalize it
        //      resolve() <- uploadRoot: /project/public/uploads +
        //      relativePath: /images/cat.png =
        //      /project/public/uploads/images/cat.png
        std::error_code error;
        const fs::path target = fs::weakly_canonical(Fs::resolve(uploadRoot, relativePath), error);

        // 4.   Return nullopt of path resolution failed, or normalized target outside uploadRoot
        if (error || !Fs::isPrefixOf(uploadRoot, target)) {
            return std::nullopt;
        }

        return target;
    }

} //namespace

HttpResponse UploadHandler::handle(const HttpRequest& request, const ResolvedRoute& route) {
    // validate configured upload root directory -> error 500
    std::error_code error;
    const fs::path uploadRoot = fs::canonical(route.upload->uploadPath, error);
    if (error || !fs::is_directory(uploadRoot, error)) {
        return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
    }

    //validate client path -> error 403
    const auto target = resolveTarget(request, route, uploadRoot);
    if (!target) {
        return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
    }
}

/*
struct HttpRequest {
    HttpMethod method;

    std::string target;
!    std::string path; what file path the client requested: "/uploads/avatar.png";
    std::string query;
    std::string version;
    HttpHeaders headers;
!    std::string body; actual file data (bytes)

    bool isPersistent() const;
};

struct ResolvedRoute {
!    std::string locationPath;

    std::filesystem::path root;
    std::string index;
    std::size_t clientMaxBodySize;

    std::set<HttpMethod> allowedMethods;
    bool autoindex = false;

    std::optional<RedirectConfig> redirect;
!    std::optional<UploadConfig> upload;
    std::optional<CgiConfig> cgi;

    std::unordered_map<HttpStatus, std::filesystem::path> errorPages;
};
*/
