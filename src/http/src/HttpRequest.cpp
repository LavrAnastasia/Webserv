#include "http/HttpRequest.hpp"
#include "HttpHeadersUtils.hpp"

#include <string>
#include <string_view>

namespace {
    namespace ConnectionOption {
        constexpr std::string_view Close = "close";
    }
} // namespace

bool HttpRequest::isPersistent() const {
    const std::optional<std::string> value = headers.get(std::string(Http::Headers::Connection));

    return !value || !HttpHeaders::equals(*value, ConnectionOption::Close);
}
