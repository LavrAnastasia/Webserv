#include <charconv>
#include <string_view>

#include "HeadersParser.hpp"
#include "HttpHeadersUtils.hpp"
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
    : _buffer(), _state(ParserState::StartLine), _failure(), _request(), _contentLength(0), _currentChunkSize(0) {
}

HttpParser::Step HttpParser::fail(HttpStatus status) {
    _failure = status;

    return Step::Continue;
}

ParseResult HttpParser::append(const char* data, std::size_t size) {
    _buffer.append(data, size);

    while (true) {
        Step step = Step::WaitForData;

        switch (_state) {
            case ParserState::StartLine:
                step = handleStartLine();
                break;
            case ParserState::Headers:
                step = handleHeaders();
                break;
            case ParserState::Body:
                step = handleBody();
                break;
            case ParserState::ChunkSize:
                step = handleChunkSize();
                break;
            case ParserState::ChunkData:
                step = handleChunkData();
                break;
            case ParserState::ChunkEnd:
                step = handleChunkEnd();
                break;
            case ParserState::Complete:
                return Complete{_request};
        }

        if (_failure) {
            return Failed{*_failure};
        }

        if (step == Step::WaitForData) {
            return NeedMoreData{};
        }
    }
}

HttpParser::Step HttpParser::handleStartLine() {
    std::size_t lineEnd = _buffer.find(Http::Syntax::CRLF);

    if (lineEnd == std::string::npos) {
        if (_buffer.size() > MAX_START_LINE_SIZE) {
            return fail(HttpStatus::UriTooLong);
        }
        return Step::WaitForData;
    }

    if (lineEnd > MAX_START_LINE_SIZE) {
        return fail(HttpStatus::UriTooLong);
    }
    std::string line = _buffer.substr(0, lineEnd);
    _buffer.erase(0, lineEnd + Http::Syntax::CRLF.size());

    const RequestLineResult parsed = RequestLineParser::parse(line);

    if (const HttpStatus* status = std::get_if<HttpStatus>(&parsed)) {
        return fail(*status);
    }

    _request = *std::get_if<HttpRequest>(&parsed);
    _state = ParserState::Headers;
    return Step::Continue;
}

HttpParser::Step HttpParser::handleHeaders() {
    std::string headersBlock;

    if (_buffer.compare(0, Http::Syntax::CRLF.size(), Http::Syntax::CRLF) == 0) {
        _buffer.erase(0, Http::Syntax::CRLF.size());
    } else {
        std::size_t headersEnd = _buffer.find(Http::Syntax::HeaderSectionEnd);
        if (headersEnd == std::string::npos) {
            if (_buffer.size() > MAX_HEADERS_SIZE) {
                return fail(HttpStatus::RequestHeaderFieldsTooLarge);
            }
            return Step::WaitForData;
        }

        if (headersEnd > MAX_HEADERS_SIZE) {
            return fail(HttpStatus::RequestHeaderFieldsTooLarge);
        }
        headersBlock = _buffer.substr(0, headersEnd);
        _buffer.erase(0, headersEnd + Http::Syntax::HeaderSectionEnd.size());
    }

    std::optional<HttpHeaders> headers = HeadersParser::parse(headersBlock);
    if (!headers) {
        return fail(HttpStatus::BadRequest);
    }

    if (!headers.value().has(std::string(Http::Headers::Host))) {
        return fail(HttpStatus::BadRequest);
    }
    _request.headers = headers.value();

    std::optional<std::string> transferEncoding = _request.headers.get(std::string(Http::Headers::TransferEncoding));

    if (transferEncoding) {
        if (Http::Ascii::tolower(*transferEncoding) == Http::TransferCoding::Chunked) {
            _state = ParserState::ChunkSize;
            return Step::Continue;
        }
        return fail(HttpStatus::NotImplemented);
    }
    const std::optional<std::size_t> contentLength = parseContentLength();

    if (!contentLength) {
        return fail(HttpStatus::BadRequest);
    }
    _contentLength = *contentLength;

    if (_contentLength > MAX_BODY_SIZE) {
        return fail(HttpStatus::PayloadTooLarge);
    }
    if (_contentLength > 0) {
        _state = ParserState::Body;
    } else {
        _state = ParserState::Complete;
    }
    return Step::Continue;
}

HttpParser::Step HttpParser::handleBody() {
    if (_buffer.size() < _contentLength) {
        return Step::WaitForData;
    }

    _request.body = _buffer.substr(0, _contentLength);
    _buffer.erase(0, _contentLength);
    _state = ParserState::Complete;
    return Step::Continue;
}

HttpParser::Step HttpParser::handleChunkSize() {
    std::size_t lineEnd = _buffer.find(Http::Syntax::CRLF);

    if (lineEnd == std::string::npos) {
        if (_buffer.size() > MAX_CHUNK_SIZE_LINE_SIZE) {
            return fail(HttpStatus::BadRequest);
        }
        return Step::WaitForData;
    }

    if (lineEnd > MAX_CHUNK_SIZE_LINE_SIZE) {
        return fail(HttpStatus::BadRequest);
    }

    std::string sizeLine = _buffer.substr(0, lineEnd);
    _buffer.erase(0, lineEnd + Http::Syntax::CRLF.size());

    std::size_t end = sizeLine.find(Http::Syntax::ChunkExtSeparator);
    std::string sizePart = sizeLine.substr(0, end);

    if (sizePart.empty()) {
        return fail(HttpStatus::BadRequest);
    }

    std::optional<std::size_t> chunkSize = parseUnsigned(sizePart, 16);

    if (!chunkSize) {
        return fail(HttpStatus::BadRequest);
    }
    _currentChunkSize = *chunkSize;
    if (_currentChunkSize > MAX_BODY_SIZE - _request.body.size()) {
        return fail(HttpStatus::PayloadTooLarge);
    }

    if (_currentChunkSize == 0) {
        _state = ParserState::ChunkEnd;
        return Step::Continue;
    }

    _state = ParserState::ChunkData;
    return Step::Continue;
}

HttpParser::Step HttpParser::handleChunkEnd() {
    if (_buffer.size() < Http::Syntax::CRLF.size()) {
        return Step::WaitForData;
    }

    if (_buffer.compare(0, Http::Syntax::CRLF.size(), Http::Syntax::CRLF) != 0) {
        return fail(HttpStatus::BadRequest);
    }

    _buffer.erase(0, Http::Syntax::CRLF.size());
    _state = ParserState::Complete;
    return Step::Continue;
}

HttpParser::Step HttpParser::handleChunkData() {
    if (_buffer.size() < _currentChunkSize)
        return Step::WaitForData;
    if (_buffer.size() - _currentChunkSize < Http::Syntax::CRLF.size())
        return Step::WaitForData;
    if (_buffer.compare(_currentChunkSize, Http::Syntax::CRLF.size(), Http::Syntax::CRLF) != 0) {
        return fail(HttpStatus::BadRequest);
    }

    _request.body += _buffer.substr(0, _currentChunkSize);
    _buffer.erase(0, _currentChunkSize + Http::Syntax::CRLF.size());

    _state = ParserState::ChunkSize;
    return Step::Continue;
}

std::optional<std::size_t> HttpParser::parseContentLength() const {
    const std::optional<std::string> value = _request.headers.get(std::string(Http::Headers::ContentLength));

    if (!value) {
        return 0;
    }

    return parseUnsigned(*value, 10);
}

void HttpParser::reset() {
    _state = ParserState::StartLine;
    _failure = std::nullopt;
    _request = HttpRequest();
    _contentLength = 0;
    _currentChunkSize = 0;
}
