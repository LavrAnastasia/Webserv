#pragma once

#include <optional>
#include <string>
#include <variant>

#include "http/HttpRequest.hpp"
#include "http/HttpStatus.hpp"

struct NeedMoreData {};
struct Complete {
    HttpRequest request;
};
struct Failed {
    HttpStatus status;
};

enum class ParserState {
    StartLine,
    Headers,
    Body,
    ChunkSize,
    ChunkData,
    ChunkEnd,
    Complete,
};

using ParseResult = std::variant<NeedMoreData, Complete, Failed>;

class HttpParser {
private:
    enum class Step { Continue, WaitForData };

    std::string _buffer;
    ParserState _state;
    std::optional<HttpStatus> _failure;
    HttpRequest _request;
    std::size_t _contentLength;
    std::size_t _currentChunkSize;

    Step fail(HttpStatus status);
    Step handleStartLine();
    Step handleHeaders();
    Step handleBody();
    Step handleChunkSize();
    Step handleChunkData();
    Step handleChunkEnd();
    bool loadContentLength();

public:
    HttpParser();
    ParseResult append(const char* data, std::size_t size);
    void reset();
};
