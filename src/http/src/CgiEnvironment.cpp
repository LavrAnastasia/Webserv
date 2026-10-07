#include "CgiEnvironment.hpp"

#include "HeaderFields.hpp"
#include "http/HttpMethod.hpp"
#include "net/Connection.hpp"

#include <cctype>
#include <cstddef>
#include <string_view>
#include <utility>

namespace {

    void addVariable(std::vector<std::string>& env, std::string_view name, const std::string& value) {
        std::string variable(name);
        variable += '=';
        variable += value;
        env.push_back(std::move(variable));
    }

    std::string headerVariableName(std::string_view name) {
        std::string result = "HTTP_";

        // pre-allocate memory to avoid unnecessary reallocations while appending
        result.reserve(result.size() + name.size());

        for (const unsigned char c : name) {
            if (c == '-') {
                result += '_';
            } else {
                result += static_cast<char>(std::toupper(c));
            }
        }

        return result;
    }

    std::string serverName(const HttpRequest& request) {
        const auto host = request.headers.get(Http::Headers::Host);

        if (!host) {
            return "";
        }

        const std::size_t colon = host->rfind(':');

        if (colon != std::string::npos && host->find(']', colon) == std::string::npos) {
            return host->substr(0, colon);
        }

        return *host;
    }

    void addHttpHeaders(std::vector<std::string>& env, const HttpHeaders& headers) {
        for (const std::string& name : headers.names()) {
            // ContentType and ContentLength handled separately in build(), Proxy skipped intentionally
            if (HttpHeaders::equals(name, Http::Headers::ContentType) ||
                HttpHeaders::equals(name, Http::Headers::ContentLength) ||
                HttpHeaders::equals(name, Http::Headers::Proxy)) {
                continue;
            }

            const auto value = headers.get(name);

            if (value) {
                addVariable(env, headerVariableName(name), *value);
            }
        }
    }

} // namespace

std::vector<std::string> CgiEnvironment::build(
    const HttpRequest& request, const std::filesystem::path& script, const ConnectionInfo& connectionInfo
) {
    std::vector<std::string> env;

    addVariable(env, "REQUEST_METHOD", Http::Method::toString(request.method));
    addVariable(env, "QUERY_STRING", request.query);

    addVariable(env, "SCRIPT_NAME", request.path);
    addVariable(env, "SCRIPT_FILENAME", script.string());
    addVariable(env, "PATH_INFO", "");

    addVariable(env, "SERVER_PROTOCOL", request.version);
    addVariable(env, "SERVER_SOFTWARE", "webserv");
    addVariable(env, "SERVER_NAME", serverName(request));
    addVariable(env, "SERVER_PORT", std::to_string(connectionInfo.serverPort));
    addVariable(env, "GATEWAY_INTERFACE", "CGI/1.1");
    addVariable(env, "REDIRECT_STATUS", "200");
    addVariable(env, "REMOTE_ADDR", connectionInfo.remoteAddr);

    if (!request.body.empty()) {
        addVariable(env, "CONTENT_LENGTH", std::to_string(request.body.size()));
    }

    if (const auto contentType = request.headers.get(Http::Headers::ContentType)) {
        addVariable(env, "CONTENT_TYPE", *contentType);
    }

    addHttpHeaders(env, request.headers);

    return env;
}
