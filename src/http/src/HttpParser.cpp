#include "http/HttpParser.hpp"
#include "HttpUtils.hpp"
#include <limits>

#include "RequestLineParser.hpp"

namespace {
    constexpr std::size_t MAX_START_LINE_SIZE = 8192;
    constexpr std::size_t MAX_HEADERS_SIZE = 32768;
    constexpr std::size_t MAX_BODY_SIZE = 10485760;
    constexpr std::size_t MAX_CHUNK_SIZE_LINE_SIZE = 1024;
} // namespace

HttpParser::HttpParser()
    : _buffer(), _state(ParserState::StartLine), _request(), _contentLength(0), _currentChunkSize(0) {
}

ParseResult HttpParser::append(const char* data, std::size_t size) {
    _buffer.append(data, size);
    bool progressed = true;
    while (progressed) {
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
    }
    return {ParseStatus::NeedMoreData, std::nullopt};
}

bool HttpParser::handleStartLine() {
    std::size_t lineEnd = _buffer.find(Http::Syntax::CrLf);

    if (lineEnd == std::string::npos) {
        if (_buffer.size() > MAX_START_LINE_SIZE) {
            _state = ParserState::Error;
            return true;
        }
        return false;
    }

    if (lineEnd > MAX_START_LINE_SIZE) {
        _state = ParserState::Error;
        return true;
    }
    std::string line = _buffer.substr(0, lineEnd);
    _buffer.erase(0, lineEnd + Http::Syntax::CrLf.size());

    std::optional<HttpRequest> request = RequestLineParser::parse(line);
    if (!request) {
        _state = ParserState::Error;
        return true;
    }
    _request = *request;
    _state = ParserState::Headers;
    return true;
}


bool HttpParser::handleHeaders() {
    std::size_t headersEnd = _buffer.find(Http::Syntax::HeaderSectionEnd);
    if (headersEnd == std::string::npos) {
        if (_buffer.size() > MAX_HEADERS_SIZE) {
            _state = ParserState::Error;
            return true;
        }
        return false;
    }

    if (headersEnd > MAX_HEADERS_SIZE) {
        _state = ParserState::Error;
        return true;
    }
    std::string headersBlock = _buffer.substr(0, headersEnd);
    _buffer.erase(0, headersEnd + Http::Syntax::HeaderSectionEnd.size());

    if (!_request.headers.parseHeadersBlock(headersBlock)) {
        _state = ParserState::Error;
        return true;
    }
    std::optional<std::string> transferEncoding = _request.headers.get("Transfer-Encoding");

    if (transferEncoding) {
        if (Http::Ascii::tolower(*transferEncoding) == "chunked") {
            _state = ParserState::ChunkSize;
            return true;
        }
        _state = ParserState::Error;
        return true;
    }
    if (!loadContentLength()) {
        _state = ParserState::Error;
        return true;
    }
    if (_contentLength > MAX_BODY_SIZE) {
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
    if (_buffer.size() < _contentLength) {
        return false;
    }

    _request.body = _buffer.substr(0, _contentLength);
    _buffer.erase(0, _contentLength);
    _state = ParserState::Complete;
    return true;
}

bool HttpParser::handleChunkSize() {
    std::size_t lineEnd = _buffer.find(Http::Syntax::CrLf);

    if (lineEnd == std::string::npos) {
        if (_buffer.size() > MAX_CHUNK_SIZE_LINE_SIZE) {
            _state = ParserState::Error;
            return true;
        }
        return false;
    }

    if (lineEnd > MAX_CHUNK_SIZE_LINE_SIZE) {
        _state = ParserState::Error;
        return true;
    }

    std::string sizeLine = _buffer.substr(0, lineEnd);
    _buffer.erase(0, lineEnd + Http::Syntax::CrLf.size());

    std::optional<std::size_t> chunkSize = parseChunkSize(sizeLine);
    if (!chunkSize) {
        _state = ParserState::Error;
        return true;
    }

    _currentChunkSize = *chunkSize;

    if (_currentChunkSize == 0) {
        if (_buffer.size() < Http::Syntax::CrLf.size()) {
            return false;
        }

        if (_buffer.compare(0, Http::Syntax::CrLf.size(), Http::Syntax::CrLf) != 0) {
            _state = ParserState::Error;
            return true;
        }

        _buffer.erase(0, Http::Syntax::CrLf.size());
        _state = ParserState::Complete;
        return true;
    }

    _state = ParserState::ChunkData;
    return true;
}

bool HttpParser::handleChunkData() {
    if (_buffer.size() < _currentChunkSize)
        return false;
    if (_buffer.size() - _currentChunkSize < Http::Syntax::CrLf.size())
        return false;
    if (_buffer.compare(_currentChunkSize, Http::Syntax::CrLf.size(), Http::Syntax::CrLf) != 0) {
        _state = ParserState::Error;
        return true;
    }

    if (_currentChunkSize > MAX_BODY_SIZE || _request.body.size() > MAX_BODY_SIZE - _currentChunkSize) {
        _state = ParserState::Error;
        return true;
    }

    _request.body += _buffer.substr(0, _currentChunkSize);
    _buffer.erase(0, _currentChunkSize + Http::Syntax::CrLf.size());

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
    std::size_t end = value.find(';');
    std::string sizePart = value.substr(0, end);

    if (sizePart.empty())
        return std::nullopt;

    std::size_t result = 0;
    std::size_t index = 0;

    while (index < sizePart.size()) {
        char c = sizePart[index];
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
