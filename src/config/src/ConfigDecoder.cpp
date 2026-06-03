#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <system_error>

#include "ConfigDecoder.hpp"
#include "http/HttpMethod.hpp"

namespace {
    std::string decodeHost(std::string_view value) {
        if (value == "localhost") {
            return "127.0.0.1";
        }

        if (std::ranges::count(value, '.') != 3) {
            throw std::runtime_error("Invalid listen host: " + std::string(value));
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
                throw std::runtime_error("Invalid listen host: " + std::string(value));
            }

            start = end + 1;
        }

        return std::string(value);
    }

    std::uint16_t decodePort(std::string_view value) {
        unsigned int port = 0;

        const auto [ptr, error] = std::from_chars(value.data(), value.data() + value.size(), port);

        if (error != std::errc{} || ptr != value.data() + value.size() || port == 0 || port > 65535) {
            throw std::runtime_error("Invalid listen port");
        }

        return static_cast<std::uint16_t>(port);
    }

    bool isRedirectTarget(std::string_view target) {
        return target.starts_with("/") || target.starts_with("http://") || target.starts_with("https://");
    }
} // namespace

std::string ConfigDecoder::decodeLocationPath(std::string_view value) {
    if (value.empty() || value.front() != '/') {
        throw std::runtime_error("Invalid location path: '" + std::string(value) + "'");
    }

    return std::string(value);
}

ListenConfig ConfigDecoder::decodeListen(std::string_view value) {
    const std::size_t separator = value.find(':');

    if (separator == std::string_view::npos || separator == 0 || separator == value.size() - 1 ||
        value.find(':', separator + 1) != std::string_view::npos) {
        throw std::runtime_error("Invalid listen endpoint format: " + std::string(value));
    }

    const std::string_view hostValue = value.substr(0, separator);
    const std::string_view portValue = value.substr(separator + 1);

    return ListenConfig{.host = decodeHost(hostValue), .port = decodePort(portValue)};
}

std::filesystem::path ConfigDecoder::decodeRoot(std::string_view value) {
    const std::filesystem::path path{value};

    if (path.empty()) {
        throw std::runtime_error("Root path must not be empty");
    }

    return path;
}

std::string ConfigDecoder::decodeIndex(std::string_view value) {
    if (value.empty()) {
        throw std::runtime_error("Index file name must not be empty");
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

    throw std::runtime_error("unsuppoorted autoindex value: " + std::string(value));
}

std::size_t ConfigDecoder::decodeClientMaxBodySize(std::string_view value) {
    if (value.empty()) {
        throw std::runtime_error("Client max body size must not be empty");
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
        throw std::runtime_error("Invalid client_max_body_size value");
    }

    if (size > std::numeric_limits<std::size_t>::max() / multiplier) {
        throw std::runtime_error("Client max body size is too large");
    }

    return size * multiplier;
}

std::unordered_map<int, std::filesystem::path> ConfigDecoder::decodeErrorPage(const std::vector<std::string>& values) {
    const std::filesystem::path path{values.back()};

    if (path.empty()) {
        throw std::runtime_error("Error page path must not be empty");
    }

    std::unordered_map<int, std::filesystem::path> pages;

    for (std::size_t index = 0; index < values.size() - 1; ++index) {
        int statusCode = 0;

        const std::string& value = values[index];
        const auto [ptr, error] = std::from_chars(value.data(), value.data() + value.size(), statusCode);

        if (error != std::errc{} || ptr != value.data() + value.size() || statusCode < 400 || statusCode > 599) {
            throw std::runtime_error("Invalid error_page status code: " + value);
        }

        if (!pages.emplace(statusCode, path).second) {
            throw std::runtime_error("Duplicate error_page status code: " + value);
        }
    }

    return pages;
}

std::unordered_set<HttpMethod> ConfigDecoder::decodeMethods(const std::vector<std::string>& values) {
    std::unordered_set<HttpMethod> methods;

    for (const std::string& value : values) {
        HttpMethod method;

        if (value == "GET") {
            method = HttpMethod::Get;
        } else if (value == "POST") {
            method = HttpMethod::Post;
        } else if (value == "DELETE") {
            method = HttpMethod::Delete;
        } else {
            throw std::runtime_error("Unsupported HTTP method: " + value);
        }

        if (!methods.insert(method).second) {
            throw std::runtime_error("Duplicate HTTP method: " + value);
        }
    }

    return methods;
}

RedirectConfig ConfigDecoder::decodeRedirect(std::string_view status, std::string_view target) {
    int statusCode = 0;

    const auto [ptr, error] = std::from_chars(status.data(), status.data() + status.size(), statusCode);

    if (error != std::errc{} || ptr != status.data() + status.size() || statusCode < 300 || statusCode > 399) {
        throw std::runtime_error("Invalid redirect status code: " + std::string(status));
    }

    if (!isRedirectTarget(target)) {
        throw std::runtime_error("Invalid redirect target: " + std::string(target));
    }

    return RedirectConfig{.statusCode = statusCode, .target = std::string(target)};
}

UploadConfig ConfigDecoder::decodeUpload(std::string_view value) {
    const std::filesystem::path path{value};

    if (path.empty()) {
        throw std::runtime_error("Upload path must not be empty");
    }

    return UploadConfig{.uploadPath = path};
}

CgiConfig ConfigDecoder::decodeCgi(std::string_view extension, std::string_view interpreter) {
    if (extension.empty() || extension.front() != '.') {
        throw std::runtime_error("Invalid CGI extension: " + std::string(extension));
    }

    const std::filesystem::path interpreterPath{interpreter};

    if (interpreterPath.empty()) {
        throw std::runtime_error("CGI interpreter path must not be empty");
    }

    return CgiConfig{.extension = std::string(extension), .interpreter = interpreterPath};
}
