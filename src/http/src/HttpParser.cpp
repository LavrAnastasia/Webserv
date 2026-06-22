#include "http/HttpParser.hpp"

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


                if (_contentLength > 0) {
                    _state = ParserState::Body;
                } else {
                    _state = ParserState::Complete;
                }
                break;
            }
        }
    }
}
