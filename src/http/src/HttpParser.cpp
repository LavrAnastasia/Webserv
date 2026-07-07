#include <charconv>
#include <string_view>

#include "HeadersParser.hpp"
#include "HttpSyntax.hpp"
#include "HttpUtils.hpp"
#include "RequestLineParser.hpp"
#include "http/HttpParser.hpp"

namespace {
    constexpr std::size_t MAX_START_LINE_SIZE = 8192;
    constexpr std::size_t MAX_HEADERS_SIZE = 32768;
    constexpr std::size_t MAX_BODY_SIZE = 10485760;
    constexpr std::size_t MAX_CHUNK_SIZE_LINE_SIZE = 1024;

    std::optional<std::size_t> parseUnsigned(std::string_view text, int base) {
        std::size_t value{};

        const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value, base);

        if (ec != std::errc{} || ptr != text.data() + text.size()) {
            return std::nullopt;
        }

        return value;
    }

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
    std::size_t lineEnd = _buffer.find(Http::Syntax::CRLF);

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
    _buffer.erase(0, lineEnd + Http::Syntax::CRLF.size());

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
    std::string headersBlock;

    if (_buffer.compare(0, Http::Syntax::CRLF.size(), Http::Syntax::CRLF) == 0) {
        _buffer.erase(0, Http::Syntax::CRLF.size());
    } else {
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
        headersBlock = _buffer.substr(0, headersEnd);
        _buffer.erase(0, headersEnd + Http::Syntax::HeaderSectionEnd.size());
    }

    std::optional<HttpHeaders> headers = HeadersParser::parse(headersBlock);
    if (!headers) {
        _state = ParserState::Error;
        return true;
    }

    if (!headers.value().has(std::string(Http::Header::Host))) {
        _state = ParserState::Error;
        return true;
    }
    _request.headers = headers.value();

    std::optional<std::string> transferEncoding = _request.headers.get(std::string(Http::Header::TransferEncoding));

    if (transferEncoding) {
        if (Http::Ascii::tolower(*transferEncoding) == Http::Header::ChunkedValue) {
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
    std::size_t lineEnd = _buffer.find(Http::Syntax::CRLF);

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
    _buffer.erase(0, lineEnd + Http::Syntax::CRLF.size());

    std::size_t end = sizeLine.find(Http::Syntax::ChunkExtSeparator);
    std::string sizePart = sizeLine.substr(0, end);

    if (sizePart.empty()) {
        _state = ParserState::Error;
        return true;
    }

    std::optional<std::size_t> chunkSize = parseUnsigned(sizePart, 16);

    if (!chunkSize) {
        _state = ParserState::Error;
        return true;
    }
    _currentChunkSize = *chunkSize;
    if (_currentChunkSize > MAX_BODY_SIZE - _request.body.size()) {
        _state = ParserState::Error;
        return true;
    }

    if (_currentChunkSize == 0) {
        if (_buffer.size() < Http::Syntax::CRLF.size()) {
            return false;
        }

        if (_buffer.compare(0, Http::Syntax::CRLF.size(), Http::Syntax::CRLF) != 0) {
            _state = ParserState::Error;
            return true;
        }

        _buffer.erase(0, Http::Syntax::CRLF.size());
        _state = ParserState::Complete;
        return true;
    }

    _state = ParserState::ChunkData;
    return true;
}

bool HttpParser::handleChunkData() {
    if (_buffer.size() < _currentChunkSize)
        return false;
    if (_buffer.size() - _currentChunkSize < Http::Syntax::CRLF.size())
        return false;
    if (_buffer.compare(_currentChunkSize, Http::Syntax::CRLF.size(), Http::Syntax::CRLF) != 0) {
        _state = ParserState::Error;
        return true;
    }

    _request.body += _buffer.substr(0, _currentChunkSize);
    _buffer.erase(0, _currentChunkSize + Http::Syntax::CRLF.size());

    _state = ParserState::ChunkSize;
    return true;
}

bool HttpParser::loadContentLength() {
    std::optional<std::string> contentLengthValue = _request.headers.get(std::string(Http::Header::ContentLength));

    if (!contentLengthValue) {
        _contentLength = 0;
        return true;
    }

    std::optional<std::size_t> contentLength = parseUnsigned(*contentLengthValue, 10);

    if (!contentLength)
        return false;

    _contentLength = *contentLength;
    return true;
}
