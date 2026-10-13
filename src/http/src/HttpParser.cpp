#include <algorithm>
#include <charconv>
#include <limits>
#include <string_view>
#include <utility>

#include "HeaderFields.hpp"
#include "HeadersParser.hpp"
#include "HttpSyntax.hpp"
#include "HttpUtils.hpp"
#include "RequestLineParser.hpp"
#include "http/HttpParser.hpp"

namespace {
    constexpr std::size_t MAX_BODY_RESERVE = 128u * 1024u * 1024u;
    constexpr std::size_t MAX_START_LINE_SIZE = 8192;
    constexpr std::size_t MAX_HEADERS_SIZE = 32768;
    constexpr std::size_t MAX_CHUNK_SIZE_LINE_SIZE = 1024;

    bool exceedsLimit(std::string_view buffer, std::size_t limit, std::string_view delimiter) {
        if (buffer.size() <= limit) {
            return false;
        }

        for (std::size_t size = delimiter.size() - 1; size > 0; --size) {
            if (buffer.ends_with(delimiter.substr(0, size))) {
                return buffer.size() - size > limit;
            }
        }
        return true;
    }

    std::optional<std::size_t> parseUnsigned(std::string_view text, int base) {
        std::size_t value{};

        const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value, base);

        if (ec != std::errc{} || ptr != text.data() + text.size()) {
            return std::nullopt;
        }

        return value;
    }

} // namespace

HttpParser::HttpParser(std::size_t maxBodySize)
    : _buffer(), _state(ParserState::StartLine), _failure(), _request(), _contentLength(0), _currentChunkSize(0),
      _maxBodySize(maxBodySize), _received(0) {
}

HttpParser::Step HttpParser::fail(HttpStatus status) {
    _failure = status;

    return Step::Continue;
}

ParseResult HttpParser::append(const char* data, std::size_t size) {
    if (_discard && _failure) {
        return Failed{*_failure};
    }
    if (size != 0) {
        _buffer.append(data, size);
    }

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
            case ParserState::ChunkDelimiter:
                step = handleChunkDelimiter();
                break;
            case ParserState::Complete:
                return Complete{std::move(_request)};
        }

        if (_failure) {
            return Failed{*_failure};
        }

        if (step == Step::BodyData) {
            return BodyData{std::move(_chunk)};
        }

        if (step == Step::HeadersReady) {
            return HeadersReady{&_request};
        }

        if (step == Step::WaitForData) {
            return NeedMoreData{};
        }
    }
}

HttpParser::Step HttpParser::handleStartLine() {
    std::size_t lineEnd = _buffer.find(Http::Syntax::CRLF);

    if (lineEnd == std::string::npos) {
        if (exceedsLimit(_buffer, MAX_START_LINE_SIZE, Http::Syntax::CRLF)) {
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
            if (exceedsLimit(_buffer, MAX_HEADERS_SIZE, Http::Syntax::HeaderSectionEnd)) {
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
        if (Http::Ascii::tolower(*transferEncoding) == Http::Headers::TransferCoding::Chunked) {
            _state = ParserState::ChunkSize;
            return Step::HeadersReady;
        }
        return fail(HttpStatus::NotImplemented);
    }
    const std::optional<std::size_t> contentLength = parseContentLength();

    if (!contentLength) {
        return fail(HttpStatus::BadRequest);
    }
    _contentLength = *contentLength;

    if (_contentLength > 0) {
        _state = ParserState::Body;
    } else {
        _state = ParserState::Complete;
    }
    return Step::HeadersReady;
}

HttpParser::Step HttpParser::handleBody() {
    if (!fits(_contentLength - _received)) {
        return fail(HttpStatus::PayloadTooLarge);
    }
    if (!_discard && !_stream && _received == 0 && !_buffer.empty()) {
        _request.body.reserve(std::min(_contentLength, MAX_BODY_RESERVE));
    }
    const std::size_t size = std::min(_buffer.size(), _contentLength - _received);
    collect(size);

    if (_received == _contentLength) {
        _state = ParserState::Complete;
    }
    if (_stream && size != 0) {
        return Step::BodyData;
    }
    return _received == _contentLength ? Step::Continue : Step::WaitForData;
}

HttpParser::Step HttpParser::handleChunkSize() {
    std::size_t lineEnd = _buffer.find(Http::Syntax::CRLF);

    if (lineEnd == std::string::npos) {
        if (exceedsLimit(_buffer, MAX_CHUNK_SIZE_LINE_SIZE, Http::Syntax::CRLF)) {
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
    _state = _currentChunkSize == 0 ? ParserState::ChunkEnd : ParserState::ChunkData;
    if (!fits(_currentChunkSize)) {
        return fail(HttpStatus::PayloadTooLarge);
    }

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
    const std::size_t size = std::min(_buffer.size(), _currentChunkSize);
    collect(size);
    _currentChunkSize -= size;

    if (_currentChunkSize == 0) {
        _state = ParserState::ChunkDelimiter;
    }
    if (_stream && size != 0) {
        return Step::BodyData;
    }
    return _currentChunkSize == 0 ? Step::Continue : Step::WaitForData;
}

HttpParser::Step HttpParser::handleChunkDelimiter() {
    if (_buffer.size() < Http::Syntax::CRLF.size()) {
        return Step::WaitForData;
    }
    if (!_buffer.starts_with(Http::Syntax::CRLF)) {
        return fail(HttpStatus::BadRequest);
    }
    _buffer.erase(0, Http::Syntax::CRLF.size());
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

bool HttpParser::fits(std::size_t size) const {
    return _received <= _maxBodySize && size <= _maxBodySize - _received;
}

void HttpParser::reset() {
    _state = ParserState::StartLine;
    _failure = std::nullopt;
    _request = HttpRequest();
    _contentLength = 0;
    _currentChunkSize = 0;
    _received = 0;
    _discard = false;
    _stream = false;
    _chunk.clear();
}

bool HttpParser::isIdle() const {
    return _state == ParserState::StartLine && _buffer.empty();
}

bool HttpParser::hasHeaders() const {
    return _state != ParserState::StartLine && _state != ParserState::Headers;
}

bool HttpParser::limit(std::size_t size) {
    _maxBodySize = size == 0 ? std::numeric_limits<std::size_t>::max() : size;
    return _state != ParserState::Body || fits(_contentLength - _received);
}

void HttpParser::queue(std::string_view bytes) {
    if (!bytes.empty()) {
        _buffer.append(bytes);
    }
}

bool HttpParser::isComplete() const {
    return _state == ParserState::Complete;
}

void HttpParser::discard() {
    _discard = true;
    _stream = false;
    _chunk.clear();
    _maxBodySize = std::numeric_limits<std::size_t>::max();
    std::string().swap(_request.body);
    if (_failure == HttpStatus::PayloadTooLarge) {
        _failure.reset();
    } else if (_failure) {
        _buffer.clear();
    }
}

void HttpParser::stream() {
    _stream = true;
}

void HttpParser::collect(std::size_t size) {
    if (_stream) {
        _chunk.assign(_buffer, 0, size);
    } else if (!_discard) {
        _request.body.append(_buffer, 0, size);
    }
    _buffer.erase(0, size);
    _received += size;
}
