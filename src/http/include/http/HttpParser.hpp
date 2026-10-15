#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "http/HttpRequest.hpp"
#include "http/HttpStatus.hpp"

struct NeedMoreData {};
struct HeadersReady {
    HttpRequest request;
    std::optional<std::size_t> length;
};
struct BodyData {
    std::string bytes;
};
struct Complete {};
struct Failed {
    HttpStatus status;
};

using ParseResult = std::variant<NeedMoreData, HeadersReady, BodyData, Complete, Failed>;

class HttpParser {
private:
    enum class Step { Continue, WaitForData, HeadersReady, BodyData, TooLarge };

    enum class ParserState {
        StartLine,
        Headers,
        BodyStart,
        Body,
        ChunkSize,
        ChunkData,
        ChunkEnd,
        ChunkDelimiter,
        Complete,
    };

    std::string _buffer;
    ParserState _state;
    std::optional<HttpStatus> _failure;
    HttpRequest _request;
    std::size_t _contentLength;
    std::size_t _currentChunkSize;
    std::size_t _maxBodySize;
    std::size_t _received;
    std::string _chunk;

    Step fail(HttpStatus status);
    void collect(std::size_t size);
    Step handleStartLine();
    Step handleHeaders();
    Step handleBody();
    Step handleChunkSize();
    Step handleChunkData();
    Step handleChunkEnd();
    Step handleChunkDelimiter();
    std::optional<std::size_t> parseContentLength() const;
    bool fits(std::size_t size) const;

public:
    explicit HttpParser(std::size_t maxBodySize);
    void receive(std::string_view bytes);
    ParseResult parse();
    void reset();
    void limit(std::size_t size);
    bool isIdle() const;
    bool hasHeaders() const;
};
