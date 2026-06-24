#include "http/HttpParser.hpp"
#include <limits>

HttpParser::HttpParser() : _buffer(), _state(ParserState::StartLine), _request(), _contentLength(0) {
}

ParseResult HttpParser::append(const char* data, std::size_t size) {
    _buffer.append(data, size);

    while (true) {
        switch (_state) {
            case ParserState::StartLine: {
                std::size_t lineEnd = _buffer.find("\r\n");
                if (lineEnd == std::string::npos) {
                    return {ParseStatus::NeedMoreData, std::nullopt};
                }

                std::string line = _buffer.substr(0, lineEnd);
                _buffer.erase(0, lineEnd + 2);

                std::optional<HttpRequest> request = parseRequestLine(line);
                if (!request) {
                    _state = ParserState::Error;
                    break;
                }
                _request = *request;
                _state = ParserState::Headers;
                break;
            }
            case ParserState::Headers: {
                std::size_t headersEnd = _buffer.find("\r\n\r\n");
                if (headersEnd == std::string::npos) {
                    return {ParseStatus::NeedMoreData, std::nullopt};
                }

                std::string headersBlock = _buffer.substr(0, headersEnd);
                _buffer.erase(0, headersEnd + 4);

                if (!_request.headers.parseHeadersBlock(headersBlock)) {
                    _state = ParserState::Error;
                    break;
                }
                if (!loadContentLength()) {
                    _state = ParserState::Error;
                    break;
                }

                if (_contentLength > 0) {
                    _state = ParserState::Body;
                } else {
                    _state = ParserState::Complete;
                }
                break;
            }
            case ParserState::Body:
                return {ParseStatus::NeedMoreData, std::nullopt};

            case ParserState::ChunkSize:
                return {ParseStatus::NeedMoreData, std::nullopt};

            case ParserState::ChunkData:
                return {ParseStatus::NeedMoreData, std::nullopt};

            case ParserState::Complete:
                return {ParseStatus::Complete, _request};

            case ParserState::Error:
                return {ParseStatus::BadRequest, std::nullopt};
        }
    }
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
