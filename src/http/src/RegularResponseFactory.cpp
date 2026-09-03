#include "RegularResponseFactory.hpp"

#include "ErrorResponseFactory.hpp"
#include "HttpResponseFactory.hpp"
#include "HttpStatusUtils.hpp"
#include "HttpUtils.hpp"

#include <array>
#include <cerrno>
#include <fstream>

namespace {
    namespace fs = std::filesystem;

    HttpStatus statusFromOpenError(int errorNumber) {
        if (errorNumber == 0) {
            return HttpStatus::InternalServerError;
        }
        return Http::Status::from(std::error_code(errorNumber, std::generic_category()));
    }

    std::string getContentType(const fs::path& path) {
        using MimeEntry = std::pair<std::string_view, std::string_view>;

        static constexpr std::array<MimeEntry, 16> mimeTypes = {{
            {".html", "text/html; charset=utf-8"},
            {".htm", "text/html; charset=utf-8"},
            {".css", "text/css; charset=utf-8"},
            {".js", "application/javascript"},
            {".json", "application/json"},
            {".txt", "text/plain; charset=utf-8"},
            {".xml", "application/xml"},
            {".png", "image/png"},
            {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".gif", "image/gif"},
            {".webp", "image/webp"},
            {".svg", "image/svg+xml"},
            {".ico", "image/x-icon"},
            {".pdf", "application/pdf"},
            {".wasm", "application/wasm"},
        }};

        const std::string extension = Http::Ascii::tolower(path.extension().string());

        for (const auto& [candidate, contentType] : mimeTypes) {
            if (extension == candidate) {
                return std::string(contentType);
            }
        }
        return "application/octet-stream";
    }
} // namespace

HttpResponse RegularResponseFactory::create(const fs::path& path, const ResolvedRoute& route) {
    std::ifstream file;

    errno = 0;
    file.open(path, std::ios::binary);

    const int openError = errno;

    if (!file.is_open()) {
        return ErrorResponseFactory::create(statusFromOpenError(openError), route);
    }

    std::string body;
    std::array<char, 64 * 1024> buffer{};

    try {
        while (true) {
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

            const std::streamsize bytesRead = file.gcount();

            if (bytesRead > 0) {
                body.append(buffer.data(), static_cast<std::size_t>(bytesRead));
            }

            if (file.bad()) {
                return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
            }

            if (file.eof()) {
                break;
            }

            if (file.fail()) {
                return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
            }
        }
    } catch (const std::ios_base::failure&) {
        return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
    }
    return HttpResponseFactory::create(HttpStatus::OK, std::move(body), getContentType(path));
}
