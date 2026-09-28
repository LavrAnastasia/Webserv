#include "http/HttpParser.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr))                                                                                                   \
            throw std::runtime_error(std::string("line ") + std::to_string(__LINE__) + ": " #expr);                    \
    } while (false)

constexpr std::size_t LINE_LIMIT = 8192;
constexpr std::size_t HEADERS_LIMIT = 32768;
constexpr std::size_t BODY_LIMIT = 10 * 1024 * 1024;
constexpr std::size_t CHUNK_LINE_LIMIT = 1024;

const std::string GET = "GET / HTTP/1.1\r\n";
const std::string POST = "POST /upload HTTP/1.1\r\nHost: localhost\r\n";
const std::string CHUNKED = POST + "Transfer-Encoding: chunked\r\n\r\n";

ParseResult feed(HttpParser& parser, std::string_view bytes) {
    return parser.append(bytes.data(), bytes.size());
}

ParseResult parse(const std::string& raw) {
    HttpParser parser;
    return feed(parser, raw);
}

HttpRequest complete(const ParseResult& result) {
    CHECK(std::holds_alternative<Complete>(result));
    return std::get<Complete>(result).request;
}

void needMore(const ParseResult& result) {
    CHECK(std::holds_alternative<NeedMoreData>(result));
}

void failed(const ParseResult& result, HttpStatus expected) {
    CHECK(std::holds_alternative<Failed>(result));
    CHECK(std::get<Failed>(result).status == expected);
}

std::string withLength(std::size_t size) {
    return POST + "Content-Length: " + std::to_string(size) + "\r\n\r\n";
}

void checkFragments(const std::string& raw, const std::string& body) {
    for (std::size_t split = 1; split < raw.size(); ++split) {
        HttpParser parser;
        needMore(feed(parser, std::string_view(raw).substr(0, split)));
        const auto request = complete(feed(parser, std::string_view(raw).substr(split)));
        CHECK(request.body == body);
    }

    HttpParser parser;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const auto result = feed(parser, std::string_view(raw).substr(i, 1));
        if (i + 1 == raw.size()) {
            CHECK(complete(result).body == body);
        } else {
            needMore(result);
        }
    }
}

void fullRequest() {
    const auto request = complete(parse("GET /index.html?x=1 HTTP/1.1\r\nhOsT: localhost\r\n\r\n"));
    CHECK(request.method == HttpMethod::Get);
    CHECK(request.target == "/index.html?x=1");
    CHECK(request.path == "/index.html");
    CHECK(request.query == "x=1");
    CHECK(request.version == "HTTP/1.1");
    CHECK(request.headers.get("Host").value_or("") == "localhost");
    CHECK(request.body.empty());
}

void fragmentedRequests() {
    checkFragments(GET + "Host: localhost\r\n\r\n", "");
    checkFragments(withLength(5) + "hello", "hello");
    checkFragments(CHUNKED + "4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n", "Wikipedia");
}

void contentLength() {
    CHECK(complete(parse(withLength(0))).body.empty());
    const std::string binary("a\0b", 3);
    CHECK(complete(parse(withLength(3) + binary)).body == binary);

    HttpParser parser;
    needMore(feed(parser, withLength(5) + "hel"));
    CHECK(complete(feed(parser, "lo")).body == "hello");

    HttpParser pipelined;
    CHECK(complete(feed(pipelined, withLength(5) + "hello" + GET + "Host: localhost\r\n\r\n")).body == "hello");
    pipelined.reset();
    CHECK(complete(feed(pipelined, "")).method == HttpMethod::Get);
}

void invalidContentLength() {
    for (const std::string value : {"", "-1", "+5", "abc", "5x", "1, 1", "999999999999999999999999999999999999"}) {
        failed(parse(POST + "Content-Length: " + value + "\r\n\r\n"), HttpStatus::BadRequest);
    }
}

void chunkedBody() {
    CHECK(complete(parse(CHUNKED + "0\r\n\r\n")).body.empty());
    CHECK(complete(parse(CHUNKED + "A;name=value\r\n0123456789\r\n1\r\n!\r\n0\r\n\r\n")).body == "0123456789!");
    HttpParser parser;
    needMore(feed(parser, CHUNKED + "1\r\na\r\n0\r\n"));
    CHECK(complete(feed(parser, "\r\n")).body == "a");
}

void invalidChunks() {
    for (const std::string body : {"Z\r\n", "-1\r\n", "\r\n", "1\r\naXX", "0\r\nXX"}) {
        failed(parse(CHUNKED + body), HttpStatus::BadRequest);
    }
    failed(parse(POST + "Transfer-Encoding: gzip\r\n\r\n"), HttpStatus::NotImplemented);
}

void invalidHeaders() {
    failed(parse(GET + "\r\n"), HttpStatus::BadRequest);
    for (const std::string header :
         {"BrokenHeader",
          "Bad Name: value",
          ": value",
          "Host : other",
          " X-Test: value",
          "X-Test: a\x01"
          "b",
          "hOsT: other",
          "Content-Length: 1\r\ncontent-length: 2",
          "Content-Length: 1\r\nTransfer-Encoding: chunked",
          "Transfer-Encoding: chunked\r\nContent-Length: 1"}) {
        failed(parse(GET + "Host: localhost\r\n" + header + "\r\n\r\n"), HttpStatus::BadRequest);
    }
}

std::string startLine(std::size_t size) {
    const std::string prefix = "GET /";
    const std::string suffix = " HTTP/1.1";
    return prefix + std::string(size - prefix.size() - suffix.size(), 'a') + suffix;
}

std::string headers(std::size_t size) {
    const std::string prefix = "Host: localhost\r\nX-Pad: ";
    return prefix + std::string(size - prefix.size(), 'a');
}

std::string chunkLine(std::size_t size) {
    const std::string prefix = "1;x=";
    return prefix + std::string(size - prefix.size(), 'a');
}

void lineAndHeaderLimits() {
    for (const auto size : {LINE_LIMIT - 1, LINE_LIMIT}) {
        complete(parse(startLine(size) + "\r\nHost: localhost\r\n\r\n"));
        needMore(parse(startLine(size)));
    }
    for (const std::string end : {"", "\r\nHost: localhost\r\n\r\n"}) {
        failed(parse(startLine(LINE_LIMIT + 1) + end), HttpStatus::UriTooLong);
    }
    for (const auto size : {HEADERS_LIMIT - 1, HEADERS_LIMIT}) {
        complete(parse(GET + headers(size) + "\r\n\r\n"));
        needMore(parse(GET + headers(size)));
    }
    for (const std::string end : {"", "\r\n\r\n"}) {
        failed(parse(GET + headers(HEADERS_LIMIT + 1) + end), HttpStatus::RequestHeaderFieldsTooLarge);
    }
}

void bodyLimits() {
    for (const auto size : {BODY_LIMIT - 1, BODY_LIMIT}) {
        const std::string body(size, 'a');
        CHECK(complete(parse(withLength(size) + body)).body == body);
    }
    failed(parse(withLength(BODY_LIMIT + 1)), HttpStatus::PayloadTooLarge);

    const std::string body(BODY_LIMIT, 'a');
    CHECK(complete(parse(CHUNKED + "a00000\r\n" + body + "\r\n0\r\n\r\n")).body == body);
    failed(parse(CHUNKED + "a00001\r\n"), HttpStatus::PayloadTooLarge);
    failed(parse(CHUNKED + "a00000\r\n" + body + "\r\n1\r\n"), HttpStatus::PayloadTooLarge);
}

void chunkLineLimits() {
    for (const auto size : {CHUNK_LINE_LIMIT - 1, CHUNK_LINE_LIMIT}) {
        CHECK(complete(parse(CHUNKED + chunkLine(size) + "\r\na\r\n0\r\n\r\n")).body == "a");
        needMore(parse(CHUNKED + chunkLine(size)));
    }
    for (const std::string end : {"", "\r\n"}) {
        failed(parse(CHUNKED + chunkLine(CHUNK_LINE_LIMIT + 1) + end), HttpStatus::BadRequest);
    }
}

void splitStartLineDelimiterAtLimit() {
    HttpParser parser;
    needMore(feed(parser, startLine(LINE_LIMIT) + "\r"));
    complete(feed(parser, "\nHost: localhost\r\n\r\n"));
}

void splitHeadersDelimiterAtLimit() {
    HttpParser parser;
    needMore(feed(parser, GET + headers(HEADERS_LIMIT) + "\r"));
    complete(feed(parser, "\n\r\n"));
}

void splitChunkDelimiterAtLimit() {
    HttpParser parser;
    needMore(feed(parser, CHUNKED + chunkLine(CHUNK_LINE_LIMIT) + "\r"));
    CHECK(complete(feed(parser, "\na\r\n0\r\n\r\n")).body == "a");
}

int main() {
    struct Test {
        const char* name;
        void (*run)();
    };
    const Test tests[] = {
        {"full request", fullRequest},
        {"fragmented requests", fragmentedRequests},
        {"Content-Length", contentLength},
        {"invalid Content-Length", invalidContentLength},
        {"chunked body", chunkedBody},
        {"invalid chunks", invalidChunks},
        {"invalid headers", invalidHeaders},
        {"line and header limits", lineAndHeaderLimits},
        {"body limits", bodyLimits},
        {"chunk line limits", chunkLineLimits},
        {"split start line delimiter at limit", splitStartLineDelimiterAtLimit},
        {"split headers delimiter at limit", splitHeadersDelimiterAtLimit},
        {"split chunk delimiter at limit", splitChunkDelimiterAtLimit},
    };
    int failures = 0;
    for (const auto& test : tests) {
        try {
            test.run();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cout << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }
    std::cout << "Failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
