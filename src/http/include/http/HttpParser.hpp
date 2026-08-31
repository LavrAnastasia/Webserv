#pragma once

#include <optional>
#include <string>


#include "http/HttpRequest.hpp"

enum class ParseStatus {
    NeedMoreData,
    Complete,
    BadRequest,
    ConnectionClosed,
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
    std::size_t _currentChunkSize;

    bool handleStartLine();
    bool handleHeaders();
    bool handleBody();
    bool handleChunkSize();
    bool handleChunkData();
    bool loadContentLength();

public:
    HttpParser();
    ParseResult append(const char* data, std::size_t size);
};
