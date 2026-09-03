#include "AutoindexResponseFactory.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "HttpResponseFactory.hpp"
#include "UrlCodec.hpp"

namespace {
    namespace fs = std::filesystem;

    struct DirectoryItem {
        std::string name;
        std::string href;
        bool isDirectory;
    };

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

    std::vector<DirectoryItem> getDirectoryItems(const fs::path& directoryPath) {
        std::vector<DirectoryItem> items;

        for (const fs::directory_entry& entry : fs::directory_iterator(directoryPath)) {
            const fs::file_status status = entry.symlink_status();

            DirectoryItem item;

            item.name = entry.path().filename().string();
            item.isDirectory = fs::is_directory(status);
            item.href = Http::Url::encodeSegment(item.name);

            if (item.isDirectory) {
                item.href += '/';
            }

            items.push_back(std::move(item));
        }

        return items;
    }

} // namespace

HttpResponse AutoindexResponseFactory::create(const fs::path& directoryPath, const HttpRequest& request) {
    std::vector<DirectoryItem> items = getDirectoryItems(directoryPath);

    std::sort(items.begin(), items.end(), [](const DirectoryItem& left, const DirectoryItem& right) {
        if (left.isDirectory != right.isDirectory) {
            return left.isDirectory;
        }
        return left.name < right.name;
    });

    const std::string escapedPath = escapeHtml(request.path);

    std::string body;

    body += "<!DOCTYPE html>\n";
    body += "<html lang=\"en\">\n";
    body += "<head>\n";
    body += "    <meta charset=\"UTF-8\">\n";
    body += "    <title>Index of ";
    body += escapedPath;
    body += "</title>\n";
    body += "</head>\n";
    body += "<body>\n";
    body += "    <h1>Index of ";
    body += escapedPath;
    body += "</h1>\n";
    body += "    <ul>\n";

    if (request.path != "/") {
        body += "        <li><a href=\"../\">../</a></li>\n";
    }

    for (const DirectoryItem& item : items) {
        body += "        <li><a href=\"";
        body += item.href;
        body += "\">";
        body += escapeHtml(item.name);

        if (item.isDirectory) {
            body += '/';
        }
        body += "</a></li>\n";
    }

    body += "    </ul>\n";
    body += "</body>\n";
    body += "</html>\n";
    return HttpResponseFactory::create(HttpStatus::OK, std::move(body), "text/html; charset=utf-8");
}
