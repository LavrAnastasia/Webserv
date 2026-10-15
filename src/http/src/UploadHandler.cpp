#include "UploadHandler.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

#include "ErrorResponseFactory.hpp"
#include "ErrorStatus.hpp"
#include "HeaderFields.hpp"
#include "HttpResponseFactory.hpp"
#include "HttpSyntax.hpp"
#include "UrlCodec.hpp"
#include "fs/Path.hpp"

namespace fs = std::filesystem;

namespace {
    constexpr mode_t kFileMode = S_IRUSR | S_IWUSR;
} // namespace

UploadHandler::UploadHandler(HttpRequest request, ResolvedRoute route, FileDescriptor file, fs::path path)
    : RequestHandler(std::move(request), std::move(route)), file_(std::move(file)), path_(std::move(path)) {
}

UploadHandler::~UploadHandler() {
    if (!path_.empty()) {
        std::error_code error;
        fs::remove(path_, error);
    }
}

DispatchResult UploadHandler::start(HttpRequest request, ResolvedRoute route) {
    try {
        const fs::path uploadPath = Fs::resolve(route.root, route.upload->uploadPath);
        if (!fs::is_directory(uploadPath)) {
            return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
        }
        const fs::path uploadRoot = fs::canonical(uploadPath);

        const fs::path relative = request.path.substr(route.locationPath.size());
        if (relative.empty() || relative == std::string(1, Http::Syntax::PathPrefix)) {
            return ErrorResponseFactory::create(HttpStatus::Conflict, route);
        }
        const fs::path target = fs::weakly_canonical(Fs::resolve(uploadRoot, relative));
        if (!Fs::isPrefixOf(uploadRoot, target)) {
            return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
        }
        if (fs::exists(target)) {
            return ErrorResponseFactory::create(HttpStatus::Conflict, route);
        }
        if (!fs::is_directory(target.parent_path())) {
            return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
        }

        FileDescriptor file(::open(target.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, kFileMode));
        if (!file.isOpen()) {
            return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
        }

        return std::make_unique<UploadHandler>(std::move(request), std::move(route), std::move(file), target);
    } catch (const fs::filesystem_error& error) {
        return ErrorResponseFactory::create(Http::Status::from(error.code()), route);
    }
}

bool UploadHandler::receive(std::string_view body) {
    while (!body.empty()) {
        const ssize_t written = ::write(file_.get(), body.data(), body.size());
        if (written <= 0) {
            return false;
        }
        body.remove_prefix(static_cast<std::size_t>(written));
    }
    return true;
}

HandlerResult UploadHandler::handle() {
    if (!file_.close()) {
        return reject(HttpStatus::InternalServerError);
    }
    path_.clear();
    return HttpResponseFactory::create(
        HttpStatus::Created, HttpHeaders{{Http::Headers::Location, Http::Url::encodePath(request_.path)}}
    );
}
