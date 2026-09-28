#pragma once

#include "ResolvedRoute.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

class UploadHandler {
public:
    static HttpResponse handle(const HttpRequest& request, const ResolvedRoute& route);
};
