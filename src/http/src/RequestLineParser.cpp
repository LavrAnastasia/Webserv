#include "RequestLineParser.hpp"

namespace {
    std::optional<std::vector<std::string>> requestLineTokens(const std::string& line) {
        std::vector<std::string> tokens;
        std::size_t start = 0;

        while (start < line.size()) {
            std::size_t space = line.find(' ', start);
            std::string token;

            if (space == std::string::npos) {
                token = line.substr(start);
                start = line.size();
            } else {
                token = line.substr(start, space - start);
                start = space + 1;
            }

            if (token.empty()) {
                return std::nullopt;
            }

            tokens.push_back(token);

            if (tokens.size() > 3) {
                return std::nullopt;
            }
        }

        if (tokens.size() != 3) {
            return std::nullopt;
        }

        return tokens;
    }


    bool isValidHttpVersion(const std::string& version) {
        return version == "HTTP/1.1";
    }

    bool isValidRequestTarget(const std::string& target) {
        return !target.empty() && target[0] == '/';
    }

    void fillPathAndQuery(HttpRequest& request) {
        std::size_t queryPos;
        queryPos = request.target.find('?');
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
    std::optional<std::vector<std::string>> tokens = requestLineTokens(line_);
    if (!tokens) {
        return std::nullopt;
    }

    std::optional<HttpMethod> method = parseHttpMethod((*tokens)[0]);
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
