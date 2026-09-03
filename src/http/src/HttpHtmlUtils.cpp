#include "HttpHtmlUtils.hpp"

namespace Http::Html {

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

    std::string buildPage(const std::string& title, const std::string& heading, const std::string& content) {
        std::string body;

        body += "<!DOCTYPE html>\n";
        body += "<html lang=\"en\">\n";
        body += "<head>\n";
        body += "<meta charset=\"UTF-8\">\n";
        body += "<title>";
        body += title;
        body += "</title>\n";
        body += "</head>\n";
        body += "<body>\n";
        body += "<h1>";
        body += heading;
        body += "</h1>\n";
        body += content;
        body += "</body>\n";
        body += "</html>\n";

        return body;
    }

} // namespace Http::Html
