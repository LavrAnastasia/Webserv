#include "AutoindexResponseFactory.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "ErrorResponseFactory.hpp"
#include "HttpResponseFactory.hpp"
#include "HttpUtils.hpp"

namespace {
    namespace fs = std::filesystem;

    struct DirectoryItem {
        std::string name;
        std::string href;
        bool isDirectory;
    };

    bool isUrlUnreserved(unsigned char character) {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' || character == '.' || character == '_' ||
            character == '~';
    }

    std::string encodeUrlSegment(const std::string& value) {
        constexpr char hex[] = "0123456789ABCDEF";

        std::string result;

        for (const unsigned char character : value) {
            if (isUrlUnreserved(character)) {
                result += static_cast<char>(character);
                continue;
            }

            result += '%';
            result += hex[character >> 4];
            result += hex[character & 0x0F];
        }

        return result;
    }

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
} // namespace

HttpResponse AutoindexResponseFactory::create(
    const fs::path& directoryPath, const HttpRequest& request, const ResolvedRoute& route
) {
    std::error_code error;

    fs::directory_iterator iterator(directoryPath, error);
    const fs::directory_iterator end;

    if (error) {
        return ErrorResponseFactory::create(Http::statusFromError(error), route);
    }

    std::vector<DirectoryItem> items;

    while (iterator != end) {
        const fs::directory_entry& entry = *iterator;

        const fs::file_status status = entry.symlink_status(error);

        if (error) {
            return ErrorResponseFactory::create(Http::statusFromError(error), route);
        }

        DirectoryItem item;

        item.name = entry.path().filename().string();
        item.isDirectory = fs::is_directory(status);
        item.href = encodeUrlSegment(item.name);

        if (item.isDirectory) {
            item.href += '/';
        }

        items.push_back(std::move(item));

        iterator.increment(error);

        if (error) {
            return ErrorResponseFactory::create(Http::statusFromError(error), route);
        }
    }

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
