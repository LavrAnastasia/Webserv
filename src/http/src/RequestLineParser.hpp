#pragma once

#include <string>
#include <variant>

#include "http/HttpRequest.hpp"
#include "http/HttpStatus.hpp"

using RequestLineResult = std::variant<HttpRequest, HttpStatus>;

class RequestLineParser {
private:
    explicit RequestLineParser(const std::string& line);
    RequestLineResult run();

    const std::string& line_;

public:
    static RequestLineResult parse(const std::string& line);
};
