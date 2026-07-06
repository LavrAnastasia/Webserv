#include "RequestLineParser.hpp"
#include "HttpMethodUtils.hpp"
#include "HttpUtils.hpp"

#include <algorithm>
#include <vector>


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

    bool isValidHttpVersion(const std::string& version) {
        return version == "HTTP/1.1";
    }

    bool isControlCharacter(char c) {
        unsigned char uc = static_cast<unsigned char>(c);
        return uc < 0x20 || uc == 0x7f;
    }

    bool isValidRequestTarget(const std::string& target) {
        return !target.empty() && target[0] == Http::Syntax::PathPrefix &&
            std::none_of(target.begin(), target.end(), isControlCharacter);
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

std::optional<HttpRequest> RequestLineParser::parse(const std::string& line) {
    return RequestLineParser{line}.run();
}

std::optional<HttpRequest> RequestLineParser::run() {
    std::optional<std::vector<std::string>> tokens = tokenizeRequestLine(line_);
    if (!tokens) {
        return std::nullopt;
    }

    std::optional<HttpMethod> method = Http::Method::fromString((*tokens)[0]);
    if (!method) {
        return std::nullopt;
    }

    const std::string& target = (*tokens)[1];
    const std::string& version = (*tokens)[2];

    if (!isValidRequestTarget(target)) {
        return std::nullopt;
    }

    if (!isValidHttpVersion(version)) {
        return std::nullopt;
    }

    HttpRequest request;
    request.method = *method;
    request.target = target;
    request.version = version;
    request.body = "";

    fillPathAndQuery(request);
    return request;
}
