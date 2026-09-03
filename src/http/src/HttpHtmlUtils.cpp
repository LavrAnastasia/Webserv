#include "HttpHtmlUtils.hpp"

namespace {
    std::string escape(const std::string& value) {
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

namespace Http::Html {

    std::string link(const std::string& href, const std::string& text) {
        return "<a href=\"" + escape(href) + "\">" + escape(text) + "</a>";
    }

    std::string buildPage(const std::string& title, const std::string& heading, const std::string& content) {
        std::string body;

        body += "<!DOCTYPE html>\n";
        body += "<html lang=\"en\">\n";
        body += "<head>\n";
        body += "<meta charset=\"UTF-8\">\n";
        body += "<title>";
        body += escape(title);
        body += "</title>\n";
        body += "</head>\n";
        body += "<body>\n";
        body += "<h1>";
        body += escape(heading);
        body += "</h1>\n";
        body += content;
        body += "</body>\n";
        body += "</html>\n";

        return body;
    }

} // namespace Http::Html
