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
        bool chunked = false;
    };

    struct Output {
        std::string headers;
        std::optional<ResponseBody> body;
    };

    static Output serialize(HttpResponse response, const Framing& framing);
};
