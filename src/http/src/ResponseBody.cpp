#include "http/ResponseBody.hpp"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

ResponseBody::ResponseBody() = default;

ResponseBody::ResponseBody(std::string data) : source_(std::move(data)), size_(std::get<std::string>(source_).size()) {
}

ResponseBody::ResponseBody(FileDescriptor file, std::uintmax_t size) : source_(std::move(file)), size_(size) {
}

std::variant<ResponseBody, std::error_code> ResponseBody::open(const std::filesystem::path& path) {
    struct stat status{};

    if (::stat(path.c_str(), &status) < 0) {
        return std::error_code(errno, std::generic_category());
    }

    if (!S_ISREG(status.st_mode)) {
        return std::make_error_code(std::errc::permission_denied);
    }

    if (status.st_size < 0) {
        return std::make_error_code(std::errc::io_error);
    }

    FileDescriptor file(::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC));

    if (!file.isOpen()) {
        return std::error_code(errno, std::generic_category());
    }

    return ResponseBody(std::move(file), static_cast<std::uintmax_t>(status.st_size));
}

std::uintmax_t ResponseBody::size() const {
    return size_;
}

bool ResponseBody::done() const {
    return offset_ == size_;
}

bool ResponseBody::next(std::string& out, std::size_t limit) {
    if (done()) {
        out.clear();
        return true;
    }

    if (auto* text = std::get_if<std::string>(&source_)) {
        out = std::move(*text);
        offset_ = size_;
        return true;
    }

    const auto count = static_cast<std::size_t>(std::min<std::uintmax_t>(limit, size_ - offset_));
    out.resize(count);

    const ssize_t received = ::read(std::get<FileDescriptor>(source_).get(), out.data(), count);

    if (received <= 0) {
        out.clear();
        return false;
    }

    out.resize(static_cast<std::size_t>(received));
    offset_ += out.size();
    return true;
}
