#include <algorithm>
#include <charconv>
#include <limits>
#include <system_error>

#include "ConfigDecoder.hpp"
#include "ConfigDecodingError.hpp"
#include "http/HttpMethod.hpp"

namespace {
    std::string decodeHost(std::string_view value) {
        if (value == "localhost") {
            return "127.0.0.1";
        }

        if (std::ranges::count(value, '.') != 3) {
            throw ConfigDecodingError(ConfigDecodingError::Reason::InvalidFormat, "listen host: " + std::string(value));
        }

        std::size_t start = 0;

        for (int index = 0; index < 4; ++index) {
            const std::size_t end = value.find('.', start);

            const std::string_view part =
                value.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);

            unsigned int number = 0;
            const auto [ptr, error] = std::from_chars(part.data(), part.data() + part.size(), number);

            if (error != std::errc{} || (part.size() > 1 && part.front() == '0') || ptr != part.data() + part.size() ||
                number > 255) {
                throw ConfigDecodingError(
                    ConfigDecodingError::Reason::InvalidFormat, "listen host: " + std::string(value)
                );
            }

            start = end + 1;
        }

        return std::string(value);
    }

    std::uint16_t decodePort(std::string_view value) {
        unsigned int port = 0;

        const auto [ptr, error] = std::from_chars(value.data(), value.data() + value.size(), port);

        if (error != std::errc{} || ptr != value.data() + value.size() || port == 0 || port > 65535) {
            throw ConfigDecodingError(ConfigDecodingError::Reason::InvalidFormat, "listen port: " + std::string(value));
        }

        return static_cast<std::uint16_t>(port);
    }

    bool isRedirectTarget(std::string_view target) {
        return target.starts_with("/") || target.starts_with("http://") || target.starts_with("https://");
    }
} // namespace

std::string ConfigDecoder::decodeLocationPath(std::string_view value) {
    if (value.empty() || value.front() != '/') {
        throw ConfigDecodingError(ConfigDecodingError::Reason::InvalidFormat, "path: " + std::string(value));
    }

    return std::string(value);
}

ListenConfig ConfigDecoder::decodeListen(std::string_view value) {
    const std::size_t separator = value.find(':');

    if (separator == std::string_view::npos || separator == 0 || separator == value.size() - 1 ||
        value.find(':', separator + 1) != std::string_view::npos) {
        throw ConfigDecodingError(ConfigDecodingError::Reason::InvalidFormat, "listen: " + std::string(value));
    }

    const std::string_view hostValue = value.substr(0, separator);
    const std::string_view portValue = value.substr(separator + 1);

    return ListenConfig{.host = decodeHost(hostValue), .port = decodePort(portValue)};
}

std::filesystem::path ConfigDecoder::decodeRoot(std::string_view value) {
    const std::filesystem::path path{value};

    if (path.empty()) {
        throw ConfigDecodingError(ConfigDecodingError::Reason::EmptyValue, "root");
    }

    return path;
}

std::string ConfigDecoder::decodeIndex(std::string_view value) {
    if (value.empty()) {
        throw ConfigDecodingError(ConfigDecodingError::Reason::EmptyValue, "index");
    }

    return std::string(value);
}

bool ConfigDecoder::decodeAutoIndex(std::string_view value) {
    if (value == "on") {
        return true;
    }
    if (value == "off") {
        return false;
    }

    throw ConfigDecodingError(ConfigDecodingError::Reason::UnsupportedValue, "autoindex: " + std::string(value));
}

std::size_t ConfigDecoder::decodeClientMaxBodySize(std::string_view value) {
    if (value.empty()) {
        throw ConfigDecodingError(ConfigDecodingError::Reason::EmptyValue, "client max body size");
    }

    std::size_t multiplier = 1;

    if (value.ends_with("k")) {
        multiplier = 1024;
        value.remove_suffix(1);
    } else if (value.ends_with("m")) {
        multiplier = 1024 * 1024;
        value.remove_suffix(1);
    }

    std::size_t size = 0;

    const auto [ptr, error] = std::from_chars(value.data(), value.data() + value.size(), size);

    if (error != std::errc{} || ptr != value.data() + value.size()) {
        throw ConfigDecodingError(
            ConfigDecodingError::Reason::InvalidFormat, "client max body size: " + std::string(value)
        );
    }

    if (size > std::numeric_limits<std::size_t>::max() / multiplier) {
        throw ConfigDecodingError(ConfigDecodingError::Reason::OutOfRange, "client max body size");
    }

    return size * multiplier;
}

std::unordered_map<int, std::filesystem::path> ConfigDecoder::decodeErrorPage(const std::vector<std::string>& values) {
    const std::filesystem::path path{values.back()};

    if (path.empty()) {
        throw ConfigDecodingError(ConfigDecodingError::Reason::EmptyValue, "error page path");
    }

    std::unordered_map<int, std::filesystem::path> pages;

    for (std::size_t index = 0; index < values.size() - 1; ++index) {
        int statusCode = 0;

        const std::string& value = values[index];
        const auto [ptr, error] = std::from_chars(value.data(), value.data() + value.size(), statusCode);

        if (error != std::errc{} || ptr != value.data() + value.size() || statusCode < 400 || statusCode > 599) {
            throw ConfigDecodingError(ConfigDecodingError::Reason::InvalidFormat, "error page status code: " + value);
        }

        if (!pages.emplace(statusCode, path).second) {
            throw ConfigDecodingError(ConfigDecodingError::Reason::Duplicate, "error page status code: " + value);
        }
    }

    return pages;
}

std::set<HttpMethod> ConfigDecoder::decodeMethods(const std::vector<std::string>& values) {
    std::set<HttpMethod> methods;

    for (const std::string& value : values) {
        HttpMethod method;

        if (value == "GET") {
            method = HttpMethod::Get;
        } else if (value == "POST") {
            method = HttpMethod::Post;
        } else if (value == "DELETE") {
            method = HttpMethod::Delete;
        } else {
            throw ConfigDecodingError(ConfigDecodingError::Reason::UnsupportedValue, "method: " + value);
        }

        if (!methods.insert(method).second) {
            throw ConfigDecodingError(ConfigDecodingError::Reason::Duplicate, "method: " + value);
        }
    }

    return methods;
}

RedirectConfig ConfigDecoder::decodeRedirect(std::string_view status, std::string_view target) {
    int statusCode = 0;

    const auto [ptr, error] = std::from_chars(status.data(), status.data() + status.size(), statusCode);

    if (error != std::errc{} || ptr != status.data() + status.size() || statusCode < 300 || statusCode > 399) {
        throw ConfigDecodingError(
            ConfigDecodingError::Reason::InvalidFormat, "redirect status code: " + std::string(status)
        );
    }

    if (!isRedirectTarget(target)) {
        throw ConfigDecodingError(
            ConfigDecodingError::Reason::InvalidFormat, "redirect target: " + std::string(target)
        );
    }

    return RedirectConfig{.statusCode = statusCode, .target = std::string(target)};
}

UploadConfig ConfigDecoder::decodeUpload(std::string_view value) {
    const std::filesystem::path path{value};

    if (path.empty()) {
        throw ConfigDecodingError(ConfigDecodingError::Reason::EmptyValue, "upload path");
    }

    return UploadConfig{.uploadPath = path};
}

CgiConfig ConfigDecoder::decodeCgi(std::string_view extension, std::string_view interpreter) {
    if (extension.empty() || extension.front() != '.') {
        throw ConfigDecodingError(
            ConfigDecodingError::Reason::InvalidFormat, "CGI extension: " + std::string(extension)
        );
    }

    const std::filesystem::path interpreterPath{interpreter};

    if (interpreterPath.empty()) {
        throw ConfigDecodingError(ConfigDecodingError::Reason::EmptyValue, "CGI interpreter path");
    }

    return CgiConfig{.extension = std::string(extension), .interpreter = interpreterPath};
}
