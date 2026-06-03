#include "ConfigDecoder.hpp"

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

bool ConfigDecoder::decodeAutoIndex(const std::vector<std::string>& arguments) {
    (void)arguments;

    return false;
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

std::vector<HttpMethod> ConfigDecoder::decodeMethods(const std::vector<std::string>& arguments) {
    (void)arguments;

    return {};
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
