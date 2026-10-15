#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "http/HttpRequest.hpp"
#include "http/HttpStatus.hpp"

struct NeedMoreData {};
struct HeadersData {
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

using ParseResult = std::variant<NeedMoreData, HeadersData, BodyData, Complete, Failed>;

class HttpParser {
private:
    enum class Step { Continue, WaitForData, HeadersData, BodyData };

    enum class ParserState {
        StartLine,
        Headers,
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
    std::size_t _received;
    std::string _bodyData;

    Step fail(HttpStatus status);
    void cutBody(std::size_t size);
    Step handleStartLine();
    Step handleHeaders();
    Step handleBody();
    Step handleChunkSize();
    Step handleChunkData();
    Step handleChunkEnd();
    Step handleChunkDelimiter();
    std::optional<std::size_t> parseContentLength() const;

public:
    HttpParser();
    void receive(std::string_view bytes);
    ParseResult parse();
    void reset();
    std::size_t announcedBodySize() const;
    bool isIdle() const;
    bool hasHeaders() const;
};
