#include "http/HttpParser.hpp"
#include "http/HttpUtils.hpp"
#include <limits>

HttpParser::HttpParser()
    : _buffer(), _state(ParserState::StartLine), _request(), _contentLength(0), _currentChunkSize(0) {
}

ParseResult HttpParser::append(const char* data, std::size_t size) {
    _buffer.append(data, size);
    while (true) {
        bool progressed;
        switch (_state) {
            case ParserState::StartLine:
                progressed = handleStartLine();
                break;
            case ParserState::Headers:
                progressed = handleHeaders();
                break;
            case ParserState::Body:
                progressed = handleBody();
                break;
            case ParserState::ChunkSize:
                progressed = handleChunkSize();
                break;
            case ParserState::ChunkData:
                progressed = handleChunkData();
                break;
            case ParserState::Complete:
                return {ParseStatus::Complete, _request};
            case ParserState::Error:
                return {ParseStatus::BadRequest, std::nullopt};
        }
        if (!progressed)
            return {ParseStatus::NeedMoreData, std::nullopt};
    }
}

bool HttpParser::handleStartLine() {
    std::size_t lineEnd = _buffer.find("\r\n");
    if (lineEnd == std::string::npos)
        return false;

    std::string line = _buffer.substr(0, lineEnd);
    _buffer.erase(0, lineEnd + 2);

    std::optional<HttpRequest> request = parseRequestLine(line);
    if (!request) {
        _state = ParserState::Error;
        return true;
    }
    _request = *request;
    _state = ParserState::Headers;
    return true;
}
bool HttpParser::handleHeaders() {
    std::size_t headersEnd = _buffer.find("\r\n\r\n");
    if (headersEnd == std::string::npos) {
        return false;
    }

    std::string headersBlock = _buffer.substr(0, headersEnd);
    _buffer.erase(0, headersEnd + 4);

    if (!_request.headers.parseHeadersBlock(headersBlock)) {
        _state = ParserState::Error;
        return true;
    }
    std::optional<std::string> transferEncoding = _request.headers.get("Transfer-Encoding");

    if (transferEncoding && toLowerAscii(*transferEncoding) == "chunked") {
        _state = ParserState::ChunkSize;
        return true;
    }
    if (!loadContentLength()) {
        _state = ParserState::Error;
        return true;
    }

    if (_contentLength > 0) {
        _state = ParserState::Body;
    } else {
        _state = ParserState::Complete;
    }
    return true;
}

bool HttpParser::handleBody() {
    if (_buffer.size() < _contentLength)
        return false;

    _request.body = _buffer.substr(0, _contentLength);
    _buffer.erase(0, _contentLength);
    _state = ParserState::Complete;
    return true;
}

bool HttpParser::handleChunkSize() {
    std::size_t lineEnd = _buffer.find("\r\n");

    if (lineEnd == std::string::npos)
        return false;
    std::string sizeLine = _buffer.substr(0, lineEnd);
    _buffer.erase(0, lineEnd + 2);

    std::optional<std::size_t> chunkSize = parseChunkSize(sizeLine);
    if (!chunkSize) {
        _state = ParserState::Error;
        return true;
    }
    _currentChunkSize = *chunkSize;

    if (_currentChunkSize == 0) {
        if (_buffer.size() < 2) {
            return false;
        }

        if (_buffer.substr(0, 2) != "\r\n") {
            _state = ParserState::Error;
            return true;
        }

        _buffer.erase(0, 2);
        _state = ParserState::Complete;
        return true;
    }

    _state = ParserState::ChunkData;
    return true;
}

bool HttpParser::handleChunkData() {
    if (_buffer.size() < _currentChunkSize)
        return false;
    if (_buffer.size() < _currentChunkSize < 2)
        return false;
    if (_buffer[_currentChunkSize] != '\r' || _buffer[_currentChunkSize + 1] != '\n') {
        _state = ParserState::Error;
        return true;
    }

    _request.body += _buffer.substr(0, _currentChunkSize);
    _buffer.erase(0, _currentChunkSize + 2);

    _state = ParserState::ChunkSize;
    return true;
}


std::optional<std::size_t> HttpParser::parseContentLength(const std::string& value) {
    if (value.empty())
        return std::nullopt;

    std::size_t result = 0;
    std::size_t index = 0;
    char c;

    while (index < value.size()) {
        c = value[index];
        if (c < '0' || c > '9')
            return std::nullopt;

        std::size_t digit = static_cast<std::size_t>(c - '0');

        if (result > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
            return std::nullopt;
        }

        result = result * 10 + digit;
        ++index;
    }
    return result;
}

bool HttpParser::loadContentLength() {
    std::optional<std::string> contentLengthValue = _request.headers.get("Content-Length");

    if (!contentLengthValue) {
        _contentLength = 0;
        return true;
    }
    std::optional<std::size_t> contentLength = parseContentLength(*contentLengthValue);

    if (!contentLength)
        return false;
    _contentLength = *contentLength;
    return true;
}

std::optional<std::size_t> HttpParser::parseChunkSize(const std::string& value) {
    if (value.empty())
        return std::nullopt;

    std::size_t result = 0;
    std::size_t index = 0;

    while (index < value.size()) {
        char c = value[index];
        std::size_t digit;

        if (c >= '0' && c <= '9') {
            digit = static_cast<std::size_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = static_cast<std::size_t>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            digit = static_cast<std::size_t>(c - 'A' + 10);
        } else {
            return std::nullopt;
        }

        if (result > (std::numeric_limits<std::size_t>::max() - digit) / 16)
            return std::nullopt;

        result = result * 16 + digit;
        ++index;
    }

    return result;
}
