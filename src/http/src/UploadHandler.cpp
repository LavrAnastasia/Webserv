#include "UploadHandler.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ErrorResponseFactory.hpp"
#include "ErrorStatus.hpp"
#include "HeaderFields.hpp"
#include "HttpResponseFactory.hpp"
#include "HttpSyntax.hpp"
#include "UrlCodec.hpp"
#include "fs/Path.hpp"

namespace fs = std::filesystem;

namespace {
    constexpr int kInvalidFd = -1;
    constexpr mode_t kFileMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
} // namespace

UploadHandler::UploadHandler() : fd_(kInvalidFd) {
}

UploadHandler::~UploadHandler() {
    if (fd_ != kInvalidFd) {
        ::close(fd_);
    }
    if (!path_.empty()) {
        std::error_code error;
        fs::remove(path_, error);
    }
}

std::optional<HttpStatus> UploadHandler::open(const HttpRequest& request, const ResolvedRoute& route) {
    try {
        const fs::path uploadPath = Fs::resolve(route.root, route.upload->uploadPath);
        if (!fs::is_directory(uploadPath)) {
            return HttpStatus::InternalServerError;
        }
        const fs::path uploadRoot = fs::canonical(uploadPath);

        const fs::path relative = request.path.substr(route.locationPath.size());
        if (relative.empty() || relative == std::string(1, Http::Syntax::PathPrefix)) {
            return HttpStatus::Conflict;
        }
        const fs::path target = fs::weakly_canonical(Fs::resolve(uploadRoot, relative));
        if (!Fs::isPrefixOf(uploadRoot, target)) {
            return HttpStatus::Forbidden;
        }
        if (fs::exists(target)) {
            return HttpStatus::Conflict;
        }
        if (!fs::is_directory(target.parent_path())) {
            return HttpStatus::InternalServerError;
        }

        fd_ = ::open(target.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, kFileMode);
        if (fd_ == kInvalidFd) {
            return HttpStatus::InternalServerError;
        }
        path_ = target;
        location_ = Http::Url::encodePath(request.path);
        pages_ = route.errorPages;
        return std::nullopt;
    } catch (const fs::filesystem_error& error) {
        return Http::Status::from(error.code());
    }
}

bool UploadHandler::receive(std::string_view bytes) {
    while (!bytes.empty()) {
        const ssize_t written = ::write(fd_, bytes.data(), bytes.size());
        if (written <= 0) {
            return false;
        }
        bytes.remove_prefix(static_cast<std::size_t>(written));
    }
    return true;
}

HttpResponse UploadHandler::end() {
    const int result = ::close(fd_);
    fd_ = kInvalidFd;
    if (result != 0) {
        return ErrorResponseFactory::create(HttpStatus::InternalServerError, pages_);
    }
    path_.clear();
    return HttpResponseFactory::create(HttpStatus::Created, HttpHeaders{{Http::Headers::Location, location_}});
}
