#pragma once

#include "http/HttpResponse.hpp"

class HttpSerializer {
public:
    static std::string serialize(const HttpResponse& response, bool headersOnly = false);
};
