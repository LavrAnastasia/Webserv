#include "RegularResponseFactory.hpp"

#include "ErrorResponseFactory.hpp"
#include "HttpResponseFactory.hpp"
#include "HttpStatusUtils.hpp"
#include "MimeTypes.hpp"

#include <array>
#include <cerrno>
#include <fstream>

HttpResponse RegularResponseFactory::create(const std::filesystem::path& path, const ResolvedRoute& route) {
    std::ifstream file;

    errno = 0;
    file.open(path, std::ios::binary);

    const int openError = errno;

    if (!file.is_open()) {
        const std::error_code error(openError, std::generic_category());
        return ErrorResponseFactory::create(Http::Status::from(error), route);
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

    return HttpResponseFactory::create(HttpStatus::OK, std::move(body), Http::Mime::from(path));
}
