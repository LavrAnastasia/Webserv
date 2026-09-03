#include "AutoindexResponseFactory.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "HttpHtmlUtils.hpp"
#include "HttpResponseFactory.hpp"
#include "UrlCodec.hpp"

namespace {
    namespace fs = std::filesystem;

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
            body += Http::Html::escape(name);

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

    const std::string escapedPath = Http::Html::escape(request.path);
    const std::string title = "Index of " + escapedPath;
    const std::string content = buildDirectoryList(entries, request.path);

    std::string body = Http::Html::buildPage(title, title, content);
    return HttpResponseFactory::create(HttpStatus::OK, std::move(body), "text/html; charset=utf-8");
}
