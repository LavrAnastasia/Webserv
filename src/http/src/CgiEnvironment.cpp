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


} // namespace

std::vector<std::string> CgiEnvironment::build(const HttpRequest& request, const std::filesystem::path& script) {
    std::vector<std::string> env;

    addVariable(env, "REQUEST_METHOD", Http::Method::toString(request.method));
    addVariable(env, "QUERY_STRING", request.query);
    addVariable(env, "SCRIPT_NAME", request.path);
    addVariable(env, "SCRIPT_FILENAME", script.string());
    addVariable(env, "SERVER_PROTOCOL", request.version);
    addVariable(env, "SERVER_SOFTWARE", "webserv");
    addVariable(env, "GATEWAY_INTERFACE", "CGI/1.1");

    addVariable(env, "CONTENT_LENGTH", std::to_string(request.body.size()));

    if (const auto contentType = request.headers.get(Http::Headers::ContentType)) {
        addVariable(env, "CONTENT_TYPE", *contentType);
    }

    return env;
}
