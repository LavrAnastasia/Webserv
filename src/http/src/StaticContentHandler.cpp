#include "StaticContentHandler.hpp"

#include "ErrorResponseFactory.hpp"
#include "HttpHeadersUtils.hpp"
#include "HttpHtmlUtils.hpp"
#include "HttpResponseFactory.hpp"
#include "HttpStatusUtils.hpp"
#include "HttpSyntax.hpp"
#include "MimeTypes.hpp"
#include "UrlCodec.hpp"

#include "fs/PathUtils.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <fstream>
#include <vector>


namespace {
    namespace fs = std::filesystem;

    std::string buildDirectoryList(const std::vector<fs::directory_entry>& entries, const std::string& requestPath) {
        std::string body = "<ul>\n";

        if (requestPath != "/") {
            body += "<li><a href=\"../\">../</a></li>\n";
        }

        for (const fs::directory_entry& entry : entries) {
            const std::string name = entry.path().filename().string();
            const bool isDirectory = entry.is_directory();

            std::string href = Http::Url::encodeSegment(name);

            if (isDirectory) {
                href += '/';
            }

            std::string label = name;

            if (isDirectory) {
                label += '/';
            }
            body += "<li>" + Http::Html::link(href, label) + "</li>\n";
        }

        body += "</ul>\n";

        return body;
    }

    std::vector<fs::directory_entry> getDirectoryEntries(const fs::path& directoryPath) {
        std::vector<fs::directory_entry> entries;

        for (const fs::directory_entry& entry : fs::directory_iterator(directoryPath)) {
            entries.push_back(entry);
        }

        return entries;
    }

    HttpResponse handleAutoindex(const fs::path& directoryPath, const HttpRequest& request) {
        std::vector<fs::directory_entry> entries = getDirectoryEntries(directoryPath);

        std::sort(
            entries.begin(), entries.end(), [](const fs::directory_entry& left, const fs::directory_entry& right) {
                const bool leftDirectory = left.is_directory();
                const bool rightDirectory = right.is_directory();

                if (leftDirectory != rightDirectory) {
                    return leftDirectory;
                }

                return left.path().filename().string() < right.path().filename().string();
            }
        );

        const std::string title = "Index of " + request.path;
        const std::string content = buildDirectoryList(entries, request.path);

        std::string body = Http::Html::buildPage(title, title, content);
        return HttpResponseFactory::create(HttpStatus::OK, std::move(body), std::string(Http::Mime::Html));
    }

    // TODO: WEB-34 Do not load the whole file into memory
    HttpResponse handleFileRequest(const fs::path& path, const ResolvedRoute& route) {
        std::ifstream file;

        errno = 0;
        file.open(path, std::ios::binary);

        const int openError = errno;

        if (!file.is_open()) {
            const std::error_code error(openError, std::generic_category());
            return ErrorResponseFactory::create(Http::Status::from(error), route);
        }

        std::string body;
        std::array<char, 64 * 1024> buffer{};

        while (true) {
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

            const std::streamsize bytesRead = file.gcount();

            if (bytesRead > 0) {
                body.append(buffer.data(), static_cast<std::size_t>(bytesRead));
            }

            if (file.bad()) {
                return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
            }

            if (file.eof()) {
                break;
            }

            if (file.fail()) {
                return ErrorResponseFactory::create(HttpStatus::InternalServerError, route);
            }
        }

        return HttpResponseFactory::create(HttpStatus::OK, std::move(body), Http::Mime::from(path));
    }

    HttpResponse handleRedirectRequest(const HttpRequest& request) {
        std::string location = Http::Url::encodePath(request.path);

        location += Http::Syntax::PathPrefix;

        if (!request.query.empty()) {
            location += Http::Syntax::QuerySeparator;
            location += request.query;
        }

        HttpResponse response{};

        response.status = HttpStatus::MovedPermanently;
        response.headers.set(std::string(Http::Headers::Location), location);
        response.headers.set(std::string(Http::Headers::ContentLength), "0");
        return response;
    }

    HttpResponse handleDirectoryRequest(
        const fs::path& directoryPath, const fs::path& root, const HttpRequest& request, const ResolvedRoute& route
    ) {
        if (request.path.back() != Http::Syntax::PathPrefix) {
            return handleRedirectRequest(request);
        }

        if (!route.index.empty()) {
            const fs::path indexPath = fs::weakly_canonical(directoryPath / route.index);

            if (!Fs::isPrefixOf(root, indexPath)) {
                return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
            }

            const fs::file_status indexStatus = fs::status(indexPath);

            if (fs::is_regular_file(indexStatus)) {
                return handleFileRequest(indexPath, route);
            }

            if (fs::exists(indexStatus)) {
                return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
            }
        }

        if (route.autoindex) {
            return handleAutoindex(directoryPath, request);
        }

        return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
    }
} // namespace

HttpResponse StaticContentHandler::handle(const HttpRequest& request, const ResolvedRoute& route) {
    if (request.path.empty() || request.path.front() != '/') {
        return ErrorResponseFactory::create(HttpStatus::BadRequest, route);
    }

    try {
        const fs::path root = fs::weakly_canonical(route.root);
        const fs::path filePath = fs::weakly_canonical(root / fs::path(request.path).relative_path());

        if (!Fs::isPrefixOf(root, filePath)) {
            return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
        }

        const fs::file_status fileStatus = fs::status(filePath);

        if (!fs::exists(fileStatus)) {
            return ErrorResponseFactory::create(HttpStatus::NotFound, route);
        }

        if (fs::is_directory(fileStatus)) {
            return handleDirectoryRequest(filePath, root, request, route);
        }

        if (!fs::is_regular_file(fileStatus)) {
            return ErrorResponseFactory::create(HttpStatus::Forbidden, route);
        }
        return handleFileRequest(filePath, route);
    } catch (const fs::filesystem_error& error) {
        return ErrorResponseFactory::create(Http::Status::from(error.code()), route);
    }
}
