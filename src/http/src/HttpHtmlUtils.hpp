#pragma once

#include <string>

namespace Http::Html {

    std::string escape(const std::string& value);

    std::string buildPage(const std::string& title, const std::string& heading, const std::string& content = "");

} // namespace Http::Html
