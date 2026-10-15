#pragma once

#include <string_view>

#include "ResolvedRoute.hpp"
#include "fs/FileDescriptor.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

class UploadHandler {
public:
    UploadHandler() = default;
    ~UploadHandler();
    UploadHandler(const UploadHandler&) = delete;
    UploadHandler& operator=(const UploadHandler&) = delete;

    std::optional<HttpStatus> open(const HttpRequest& request, const ResolvedRoute& route);
    bool receive(std::string_view bytes);
    HttpResponse end();

private:
    FileDescriptor file_;
    std::filesystem::path path_;
    std::string location_;
    ErrorPages pages_;
};
