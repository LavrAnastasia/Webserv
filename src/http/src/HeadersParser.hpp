#pragma once

#include "http/HttpHeaders.hpp"

#include <optional>
#include <string>

class HeadersParser {
private:
    explicit HeadersParser(const std::string& headersBlock);
    std::optional<HttpHeaders> run();

    bool parseHeaderLine(const std::string& line);

    const std::string& headersBlock_;
    HttpHeaders headers_;

public:
    static std::optional<HttpHeaders> parse(const std::string& headersBlock);
};