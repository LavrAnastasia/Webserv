#include "CgiEnvironment.hpp"

#include "HeaderFields.hpp"
#include "http/HttpMethod.hpp"

#include <cctype>
#include <set>
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

    void addHttpHeaders(std::vector<std::string>& env, const HttpHeaders& headers) {
        std::set<std::string> added;

        for (const auto& header : headers) {
            const std::string& name = header.first;

            // handled separately in build()
            if (HttpHeaders::equals(name, Http::Headers::ContentType) ||
                HttpHeaders::equals(name, Http::Headers::ContentLength)) {
                continue;
            }

            const std::string variableName = headerVariableName(name);

            // try to insert header, skip if already in the set (duplicate)
            if (!added.insert(variableName).second) {
                continue;
            }

            const auto value = headers.get(name);

            if (value) {
                addVariable(env, variableName, *value);
            }
        }
    }


} // namespace

std::vector<std::string> CgiEnvironment::build(const HttpRequest& request, const std::filesystem::path& script) {
    std::vector<std::string> env;

    addVariable(env, "REQUEST_METHOD", Http::Method::toString(request.method));
    addVariable(env, "QUERY_STRING", request.query);

    addVariable(env, "SCRIPT_NAME", request.path);
    addVariable(env, "SCRIPT_FILENAME", script.string());
    // TODO: support CGI path-info if needed
    addVariable(env, "PATH_INFO", "");

    addVariable(env, "SERVER_PROTOCOL", request.version);
    addVariable(env, "SERVER_SOFTWARE", "webserv");
    addVariable(env, "GATEWAY_INTERFACE", "CGI/1.1");

    if (!request.body.empty()) {
        addVariable(env, "CONTENT_LENGTH", std::to_string(request.body.size()));
    }

    if (const auto contentType = request.headers.get(Http::Headers::ContentType)) {
        addVariable(env, "CONTENT_TYPE", *contentType);
    }

    // TODO: add redirect status if required by a supported CGI interpreter

    addHttpHeaders(env, request.headers);

    return env;
}
