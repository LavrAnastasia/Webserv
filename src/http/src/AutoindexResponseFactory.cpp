#include "AutoindexResponseFactory.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "HttpResponseFactory.hpp"
#include "UrlCodec.hpp"

namespace {
    namespace fs = std::filesystem;

    std::string escapeHtml(const std::string& value) {
        std::string result;

        for (const char character : value) {
            switch (character) {
                case '&':
                    result += "&amp;";
                    break;
                case '<':
                    result += "&lt;";
                    break;
                case '>':
                    result += "&gt;";
                    break;
                case '"':
                    result += "&quot;";
                    break;
                case '\'':
                    result += "&#39;";
                    break;
                default:
                    result += character;
                    break;
            }
        }
        return result;
    }

    std::string buildDirectoryList(const std::vector<fs::directory_entry>& entries, const std::string& requestPath) {
        std::string body;

        body += "<ul>\n";

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

            body += "<li><a href=\"";
            body += href;
            body += "\">";
            body += escapeHtml(name);

            if (isDirectory) {
                body += '/';
            }

            body += "</a></li>\n";
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

} // namespace

HttpResponse AutoindexResponseFactory::create(const fs::path& directoryPath, const HttpRequest& request) {
    std::vector<fs::directory_entry> entries = getDirectoryEntries(directoryPath);

    std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& left, const fs::directory_entry& right) {
        const bool leftDirectory = left.is_directory();
        const bool rightDirectory = right.is_directory();

        if (leftDirectory != rightDirectory) {
            return leftDirectory;
        }

        return left.path().filename().string() < right.path().filename().string();
    });

    const std::string escapedPath = escapeHtml(request.path);

    std::string body;

    body += "<!DOCTYPE html>\n";
    body += "<html lang=\"en\">\n";
    body += "<head>\n";
    body += "<meta charset=\"UTF-8\">\n";
    body += "<title>Index of ";
    body += escapedPath;
    body += "</title>\n";
    body += "</head>\n";
    body += "<body>\n";
    body += "<h1>Index of ";
    body += escapedPath;
    body += "</h1>\n";
    body += buildDirectoryList(entries, request.path);
    body += "</body>\n";
    body += "</html>\n";
    return HttpResponseFactory::create(HttpStatus::OK, std::move(body), "text/html; charset=utf-8");
}
