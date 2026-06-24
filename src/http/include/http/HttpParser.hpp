#pragma once

#include <optional>
#include <string>


#include "http/HttpRequest.hpp"

enum class ParseStatus {
    NeedMoreData,
    Complete,
    BadRequest,
};

enum class ParserState {
    StartLine,
    Headers,
    Body,
    ChunkSize,
    ChunkData,
    Complete,
    Error,
};

struct ParseResult {
    ParseStatus status;
    std::optional<HttpRequest> request;
};

class HttpParser {
private:
    std::string _buffer;
    ParserState _state;
    HttpRequest _request;
    std::size_t _contentLength;

    std::optional<std::size_t> parseContentLength(const std::string& value);
    bool loadContentLength();

public:
    HttpParser();
    ParseResult append(const char* data, std::size_t size);
};
