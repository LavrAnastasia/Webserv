#include <algorithm>

#include "HttpSyntax.hpp"
#include "HttpUtils.hpp"

namespace Http::Ascii {
    namespace {
        bool isWhiteSpace(char c) {
            return c == Http::Syntax::SP || c == Http::Syntax::HTAB;
        }
    } // namespace

    char tolower(char c) {
        unsigned char uc = static_cast<unsigned char>(c);

        if (uc >= 'A' && uc <= 'Z') {
            return static_cast<char>(uc - 'A' + 'a');
        }

        return c;
    }
    std::string tolower(const std::string& value) {
        std::string result = value;

        std::transform(result.begin(), result.end(), result.begin(), [](char c) { return tolower(c); });

        return result;
    }

    std::string trim(const std::string& value) {
        auto start = std::find_if(value.begin(), value.end(), [](char c) { return !isWhiteSpace(c); });

        if (start == value.end()) {
            return "";
        }

        auto reverseEnd = std::find_if(value.rbegin(), value.rend(), [](char c) { return !isWhiteSpace(c); });

        auto end = reverseEnd.base();

        return std::string(start, end);
    }

} // namespace Http::Ascii


namespace Http {
    bool isNotFoundError(const std::error_code& error) {
        return error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory;
    }

    HttpStatus statusFromError(const std::error_code& error) {
        if (error == std::errc::permission_denied || error == std::errc::operation_not_permitted) {
            return HttpStatus::Forbidden;
        }

        if (isNotFoundError(error)) {
            return HttpStatus::NotFound;
        }

        return HttpStatus::InternalServerError;
    }

    bool isWithinRoot(const std::filesystem::path& path, const std::filesystem::path& root) {
        auto pathIterator = path.begin();

        for (auto rootIterator = root.begin(); rootIterator != root.end(); ++rootIterator, ++pathIterator) {
            if (pathIterator == path.end() || *pathIterator != *rootIterator) {
                return false;
            }
        }

        return true;
    }
} // namespace Http
