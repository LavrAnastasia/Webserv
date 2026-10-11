#pragma once

#include "http/HttpResponse.hpp"

#include <optional>
#include <string>

class HttpSerializer {
public:
    struct Framing {
        bool close = false;
        bool headersOnly = false;
        bool stream = false;
    };

    struct Output {
        std::string headers;
        std::optional<ResponseBody> body;
        bool chunked = false;
        bool suppressed = false;
    };

    static Output serialize(HttpResponse response, const Framing& framing);
};
