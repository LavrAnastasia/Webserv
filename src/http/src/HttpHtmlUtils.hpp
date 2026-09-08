#pragma once

#include <string>

namespace Http::Html {

    std::string link(const std::string& href, const std::string& text);

    std::string buildPage(const std::string& title, const std::string& heading, const std::string& content = "");

} // namespace Http::Html
