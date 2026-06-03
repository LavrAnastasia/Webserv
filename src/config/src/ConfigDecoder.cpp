#include <stdexcept>

#include "ConfigDecoder.hpp"
#include "http/HttpMethod.hpp"

std::string ConfigDecoder::decodeLocationPath(const std::vector<std::string>& arguments) {
    (void)arguments;

    return "";
}

ListenConfig ConfigDecoder::decodeListen(const std::vector<std::string>& arguments) {
    (void)arguments;

    return ListenConfig{};
}

std::filesystem::path ConfigDecoder::decodeRoot(const std::vector<std::string>& arguments) {
    (void)arguments;

    return std::filesystem::path{};
}

std::string ConfigDecoder::decodeIndex(const std::vector<std::string>& arguments) {
    (void)arguments;

    return "";
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

UploadConfig ConfigDecoder::decodeUpload(const std::vector<std::string>& arguments) {
    (void)arguments;

    return UploadConfig{};
}

CgiConfig ConfigDecoder::decodeCgi(const std::vector<std::string>& arguments) {
    (void)arguments;

    return CgiConfig{};
}
