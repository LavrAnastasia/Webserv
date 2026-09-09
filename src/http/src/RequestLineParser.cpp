#include <algorithm>
#include <vector>

#include "HttpSyntax.hpp"
#include "RequestLineParser.hpp"
#include "http/HttpMethod.hpp"

namespace {
    std::optional<std::vector<std::string>> tokenizeRequestLine(const std::string& line) {
        const std::size_t methodEnd = line.find(Http::Syntax::SP);
        if (methodEnd == std::string::npos) {
            return std::nullopt;
        }

        const std::size_t targetEnd = line.find(Http::Syntax::SP, methodEnd + 1);
        if (targetEnd == std::string::npos) {
            return std::nullopt;
        }

        if (line.find(Http::Syntax::SP, targetEnd + 1) != std::string::npos) {
            return std::nullopt;
        }

        std::string method = line.substr(0, methodEnd);
        std::string target = line.substr(methodEnd + 1, targetEnd - methodEnd - 1);
        std::string version = line.substr(targetEnd + 1);

        if (method.empty() || target.empty() || version.empty()) {
            return std::nullopt;
        }

        return std::vector<std::string>{method, target, version};
    }

    bool isValidHttpVersion(std::string_view version) {
        const std::size_t separator = version.find(Http::Protocol::VersionSeparator);

        if (separator == std::string_view::npos) {
            return false;
        }

        return version.substr(0, separator) == Http::Protocol::Name &&
            version.substr(separator + 1) == Http::Protocol::Version;
    }

    bool isControlCharacter(char c) {
        unsigned char uc = static_cast<unsigned char>(c);
        return uc < 0x20 || uc == 0x7f;
    }

    bool isValidRequestTarget(const std::string& target) {
        return !target.empty() && target[0] == Http::Syntax::PathPrefix &&
            std::none_of(target.begin(), target.end(), isControlCharacter);
    }

    int hexValue(char character) {
        if (character >= '0' && character <= '9') {
            return character - '0';
        }

        if (character >= 'a' && character <= 'f') {
            return character - 'a' + 10;
        }

        if (character >= 'A' && character <= 'F') {
            return character - 'A' + 10;
        }

        return -1;
    }

    std::optional<std::string> decodeUrlPath(std::string_view encodedPath) {
        std::string decodedPath;
        decodedPath.reserve(encodedPath.size());

        for (std::size_t index = 0; index < encodedPath.size(); ++index) {
            const char character = encodedPath[index];

            if (character != '%') {
                decodedPath += character;
                continue;
            }

            if (index + 2 >= encodedPath.size()) {
                return std::nullopt;
            }

            const int high = hexValue(encodedPath[index + 1]);
            const int low = hexValue(encodedPath[index + 2]);

            if (high < 0 || low < 0) {
                return std::nullopt;
            }

            const unsigned char decodedCharacter = static_cast<unsigned char>((high << 4) | low);
            if (decodedCharacter < 0x20 || decodedCharacter == 0x7F || decodedCharacter == '/' ||
                decodedCharacter == '\\') {
                return std::nullopt;
            }

            decodedPath += static_cast<char>(decodedCharacter);
            index += 2;
        }

        return decodedPath;
    }

    bool hasDotSegments(std::string_view path) {
        std::size_t begin = 0;

        while (begin <= path.size()) {
            const std::size_t end = path.find('/', begin);

            const std::size_t length = end == std::string_view::npos ? std::string_view::npos : end - begin;

            const std::string_view segment = path.substr(begin, length);

            if (segment == "." || segment == "..") {
                return true;
            }

            if (end == std::string_view::npos) {
                break;
            }

            begin = end + 1;
        }
        return false;
    }

    std::optional<std::string> decodePath(const std::string& rawPath) {
        std::optional<std::string> decodedPath = decodeUrlPath(rawPath);

        if (!decodedPath.has_value()) {
            return std::nullopt;
        }

        if (decodedPath->empty() || decodedPath->front() != Http::Syntax::PathPrefix) {
            return std::nullopt;
        }
        if (decodedPath->find("//") != std::string::npos) {
            return std::nullopt;
        }

        if (hasDotSegments(*decodedPath)) {
            return std::nullopt;
        }

        return decodedPath;
    }

    void fillPathAndQuery(HttpRequest& request) {
        std::size_t queryPos;
        queryPos = request.target.find(Http::Syntax::QuerySeparator);
        if (queryPos == std::string::npos) {
            request.path = request.target;
            request.query = "";
        } else {
            request.path = request.target.substr(0, queryPos);
            request.query = request.target.substr(queryPos + 1);
        }
    }
} // namespace

RequestLineParser::RequestLineParser(const std::string& line) : line_(line) {
}

RequestLineResult RequestLineParser::parse(const std::string& line) {
    return RequestLineParser{line}.run();
}

RequestLineResult RequestLineParser::run() {
    std::optional<std::vector<std::string>> tokens = tokenizeRequestLine(line_);
    if (!tokens) {
        return HttpStatus::BadRequest;
    }

    std::optional<HttpMethod> method = Http::Method::fromString((*tokens)[0]);
    if (!method) {
        return HttpStatus::NotImplemented;
    }

    const std::string& target = (*tokens)[1];
    const std::string& version = (*tokens)[2];

    if (!isValidRequestTarget(target)) {
        return HttpStatus::BadRequest;
    }

    if (!isValidHttpVersion(version)) {
        return HttpStatus::HttpVersionNotSupported;
    }

    HttpRequest request;
    request.method = *method;
    request.target = target;
    request.version = version;
    request.body = "";

    fillPathAndQuery(request);

    std::optional<std::string> decodedPath = decodePath(request.path);

    if (!decodedPath) {
        return HttpStatus::BadRequest;
    }
    request.path = std::move(*decodedPath);

    return request;
}
