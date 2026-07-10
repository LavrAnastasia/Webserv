#pragma once
#include <optional>
#include <string>

#include "http/HttpRequest.hpp"

class RequestLineParser {
private:
    explicit RequestLineParser(const std::string& line);
    std::optional<HttpRequest> run();

    const std::string& line_;

public:
    static std::optional<HttpRequest> parse(const std::string& line);
};
