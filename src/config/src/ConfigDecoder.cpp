#include <stdexcept>

#include "ConfigDecoder.hpp"
#include "http/HttpMethod.hpp"

std::string ConfigDecoder::decodeLocationPath(std::string_view value) {
    if (value.empty() || value.front() != '/') {
        throw std::runtime_error("Invalid location path: '" + std::string(value) + "'");
    }

    return std::string(value);
}

ListenConfig ConfigDecoder::decodeListen(const std::vector<std::string>& arguments) {
    (void)arguments;

    return ListenConfig{};
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

    throw std::runtime_error("unsuppoorted directive value: " + std::string(value));
}

std::size_t ConfigDecoder::decodeClientMaxBodySize(const std::vector<std::string>& arguments) {
    (void)arguments;

    return 0;
}

std::unordered_map<int, std::filesystem::path>
ConfigDecoder::decodeErrorPage(const std::vector<std::string>& arguments) {
    (void)arguments;

    return {};
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

RedirectConfig ConfigDecoder::decodeRedirect(const std::vector<std::string>& arguments) {
    (void)arguments;

    return RedirectConfig{};
}

UploadConfig ConfigDecoder::decodeUpload(std::string_view value) {
    const std::filesystem::path path{value};

    if (path.empty()) {
        throw std::runtime_error("Upload path must not be empty");
    }

    return UploadConfig{.uploadPath = path};
}

CgiConfig ConfigDecoder::decodeCgi(const std::vector<std::string>& arguments) {
    (void)arguments;

    return CgiConfig{};
}
