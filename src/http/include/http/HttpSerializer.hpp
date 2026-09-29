#pragma once

#include "http/HttpResponse.hpp"

class HttpSerializer {
public:
    struct Framing {
        bool close = false;
        bool headersOnly = false;
    };

    static std::string serialize(const HttpResponse& response, const Framing& framing);
};
