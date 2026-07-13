#pragma once

#include "http/HttpResponse.hpp"
#include "src/HttpUtils.hpp"

class HttpSerializer {
public:
    static std::string serialize(const HttpResponse& response, bool headersOnly = false);
};
