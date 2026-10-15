#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <variant>

#include "RequestHandler.hpp"
#include "ResolvedRoute.hpp"
#include "fs/FileDescriptor.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

class UploadHandler : public RequestHandler {
public:
    UploadHandler(HttpRequest request, ResolvedRoute route, FileDescriptor file, std::filesystem::path path);
    ~UploadHandler() override;

    static DispatchResult start(HttpRequest request, ResolvedRoute route);
    bool receive(std::string_view body) override;
    HandlerResult handle() override;

private:
    FileDescriptor file_;
    std::filesystem::path path_;
};
