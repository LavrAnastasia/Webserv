#include "http/HttpParser.hpp"

#include <gtest/gtest.h>

#include <initializer_list>
#include <variant>

namespace {

    constexpr std::size_t LINE_LIMIT = 8192;
    constexpr std::size_t HEADERS_LIMIT = 32768;
    constexpr std::size_t BODY_LIMIT = 10 * 1024 * 1024;
    constexpr std::size_t CHUNK_LINE_LIMIT = 1024;

    const std::string GET = "GET / HTTP/1.1\r\n";

    const std::string POST = "POST /upload HTTP/1.1\r\n"
                             "Host: localhost\r\n";

    const std::string CHUNKED = POST + "Transfer-Encoding: chunked\r\n\r\n";

    //Append data to an existing parser and return the parsing result
    ParseResult result(HttpParser& parser, std::string_view bytes) {
        return parser.append(bytes.empty() ? "" : bytes.data(), bytes.size());
    }

    ParseResult parse(const std::string& raw) {
        HttpParser parser;
        return result(parser, raw);
    }

    // Test invalid HTTP methods, paths, or versions without repeating headers
    std::string withRequestLine(const std::string& line) {
        return line + "\r\nHost: localhost\r\n\r\n";
    }

    std::string withHeaders(const std::string& headers) {
        return GET + "Host: localhost\r\n" + headers + "\r\n\r\n";
    }

    std::string withLength(std::size_t size) {
        return POST + "Content-Length: " + std::to_string(size) + "\r\n\r\n";
    }

    void expectBody(const ParseResult& result, const std::string& expected) {
        const auto* parsed = std::get_if<Complete>(&result);

        ASSERT_NE(parsed, nullptr) << "Expected Complete";
        EXPECT_EQ(parsed->request.body, expected);
    }

    void expectStatus(const ParseResult& result, HttpStatus expected) {
        const auto* error = std::get_if<Failed>(&result);

        ASSERT_NE(error, nullptr) << "Expected Failed";

        EXPECT_EQ(static_cast<int>(error->status), static_cast<int>(expected));
    }

    // Generators for limit testing
    // The trailing CRLF / CRLFCRLF are not included in the size

    std::string startLine(std::size_t size) {
        const std::string prefix = "GET /";
        const std::string suffix = " HTTP/1.1";

        return prefix + std::string(size - prefix.size() - suffix.size(), 'a') + suffix;
    }

    std::string headersBlock(std::size_t size) {
        const std::string prefix = "Host: localhost\r\nX-Pad: ";

        return prefix + std::string(size - prefix.size(), 'a');
    }

    std::string chunkLine(std::size_t size) {
        const std::string prefix = "1;x=";

        return prefix + std::string(size - prefix.size(), 'a');
    }

    //Full request and correct headers

    TEST(HttpParserTest, FullRequest) {
        const auto result = parse(
            "GET /index.html?x=1 HTTP/1.1\r\n"
            "hOsT: localhost\r\n"
            "\r\n"
        );

        const auto* parsed = std::get_if<Complete>(&result);
        ASSERT_NE(parsed, nullptr);

        const auto& request = parsed->request;

        EXPECT_EQ(request.method, HttpMethod::Get);
        EXPECT_EQ(request.target, "/index.html?x=1");
        EXPECT_EQ(request.path, "/index.html");
        EXPECT_EQ(request.query, "x=1");
        EXPECT_EQ(request.version, "HTTP/1.1");
        EXPECT_EQ(request.headers.get("Host").value_or(""), "localhost");
        EXPECT_TRUE(request.body.empty());
    }

    TEST(HttpParserTest, ValidHeaderValues) {
        const auto result = parse(
            GET +
            "Host: 127.0.0.1:8080\r\n"
            "X-Empty:\r\n"
            "X-Colons: a:b:c\r\n"
            "X-Tab: a\tb\r\n"
            "\r\n"
        );

        const auto* parsed = std::get_if<Complete>(&result);
        ASSERT_NE(parsed, nullptr);

        const auto& headers = parsed->request.headers;

        EXPECT_EQ(headers.get("Host").value_or(""), "127.0.0.1:8080");
        EXPECT_EQ(headers.get("X-Colons").value_or(""), "a:b:c");
        EXPECT_EQ(headers.get("X-Tab").value_or(""), "a\tb");

        const auto empty = headers.get("X-Empty");
        ASSERT_TRUE(empty.has_value());
        EXPECT_TRUE(empty->empty());
    }

    TEST(HttpParserTest, AcceptsValidHostValues) {
        for (const std::string host :
             {"",
              "localhost",
              "example.com:8080",
              "localhost:",
              "127.0.0.1:80",
              "[::1]",
              "[::1]:",
              "[2001:db8::1]:8080",
              "[::ffff:192.0.2.1]:80",
              "caf%C3%A9.example"}) {
            SCOPED_TRACE(host);
            const auto parsed = parse(GET + "Host: \t" + host + " \t\r\n\r\n");
            const auto* complete = std::get_if<Complete>(&parsed);

            ASSERT_NE(complete, nullptr);
            EXPECT_EQ(complete->request.headers.get("Host"), host);
        }
    }

    TEST(HttpParserTest, RejectsMalformedHostValues) {
        for (const std::string host :
             {"user@localhost",
              "localhost/path",
              "localhost:80:90",
              "localhost:-1",
              "localhost: 80",
              "::1",
              "[::1",
              "[[::1]]",
              "[::1]]",
              "[::1]extra",
              "[::1]:abc",
              "[::1]:80:90",
              "[local host]",
              "[::1\t]",
              "[::1/path]",
              "[::1\\path]",
              "[]",
              "example%",
              "example%2",
              "example%ZZ"}) {
            SCOPED_TRACE(host);
            expectStatus(parse(GET + "Host: " + host + "\r\n\r\n"), HttpStatus::BadRequest);
        }
    }

    //Requests expected to fail

    struct ErrorCase {
        const char* name;
        std::string raw;
        HttpStatus expected = HttpStatus::BadRequest;
    };

    class HttpParserErrorTest : public testing::TestWithParam<ErrorCase> {};

    TEST_P(HttpParserErrorTest, ReturnsExpectedStatus) {
        const auto& test = GetParam();

        expectStatus(parse(test.raw), test.expected);
    }

    const ErrorCase ERROR_CASES[] = {
        // Query string structure
        {"MissingMethod", withRequestLine("/index.html HTTP/1.1")},
        {"EmptyMethod", withRequestLine(" /index.html HTTP/1.1")},
        {"MissingTarget", withRequestLine("GET HTTP/1.1")},
        {"EmptyTarget", withRequestLine("GET  HTTP/1.1")},
        {"MissingVersion", withRequestLine("GET /index.html")},
        {"EmptyVersion", withRequestLine("GET /index.html ")},
        {"ExtraToken", withRequestLine("GET /index.html HTTP/1.1 EXTRA")},
        {"ExtraLeadingToken", withRequestLine("TRASH GET /index.html HTTP/1.1")},

        //Strict whitespace checks
        {"LeadingSpace", withRequestLine(" GET /index.html HTTP/1.1")},
        {"DoubleSpaceAfterMethod", withRequestLine("GET  /index.html HTTP/1.1")},
        {"DoubleSpaceBeforeVersion", withRequestLine("GET /index.html  HTTP/1.1")},
        {"WrongTokenOrder", withRequestLine("HTTP/1.1 GET /index.html")},

        // Request address
        {"RelativeTarget", withRequestLine("GET index.html HTTP/1.1")},
        {"FtpTarget", withRequestLine("GET ftp://example.com/file HTTP/1.1")},
        {"ControlInTarget",
         withRequestLine(
             "GET /abc\x01"
             "def HTTP/1.1"
         )},
        {"TabInTarget", withRequestLine("GET /abc\tdef HTTP/1.1")},
        {"DelInTarget",
         withRequestLine(
             "GET /abc\x7f"
             "def HTTP/1.1"
         )},

        // Percent-encoding
        {"PercentInvalidHex", withRequestLine("GET /abc%ZZ HTTP/1.1")},
        {"PercentWithoutDigits", withRequestLine("GET /abc% HTTP/1.1")},
        {"PercentOneDigit", withRequestLine("GET /abc%2 HTTP/1.1")},
        {"PercentInvalidSecondDigit", withRequestLine("GET /abc%2G HTTP/1.1")},
        {"PercentInvalidFirstDigit", withRequestLine("GET /abc%G2 HTTP/1.1")},

        // Valid but unsupported methods
        {"UnsupportedPut", withRequestLine("PUT / HTTP/1.1"), HttpStatus::NotImplemented},
        {"UnsupportedTrace", withRequestLine("TRACE / HTTP/1.1"), HttpStatus::NotImplemented},

        // Invalid method syntax
        {"MethodOpenParen", withRequestLine("GE(T / HTTP/1.1")},
        {"MethodCloseParen", withRequestLine("GE)T / HTTP/1.1")},
        {"MethodSlash", withRequestLine("GE/T / HTTP/1.1")},
        {"MethodTab", withRequestLine("GE\tT / HTTP/1.1")},

        // Correct recording of an unsupported version.
        {"UnsupportedHttp2", withRequestLine("GET / HTTP/2.0"), HttpStatus::HttpVersionNotSupported},
        {"UnsupportedHttp3", withRequestLine("GET / HTTP/3.0"), HttpStatus::HttpVersionNotSupported},

        // Invalid recording version
        {"VersionMissingMinor", withRequestLine("GET / HTTP/1")},
        {"VersionInvalidPrefix", withRequestLine("GET / HTP/1.1")},
        {"VersionLetters", withRequestLine("GET / HTTP/abc")},
        {"VersionEmptyMinor", withRequestLine("GET / HTTP/1.")},
        {"VersionExtraCharacter", withRequestLine("GET / HTTP/1.1x")},

        // Headers
        {"MissingHost", GET + "\r\n"},
        {"HeaderWithoutColon", withHeaders("BrokenHeader")},
        {"HeaderNameWithSpace", withHeaders("Bad Name: value")},
        {"EmptyHeaderName", withHeaders(": value")},
        {"SpaceBeforeColon", withHeaders("Host : other")},
        {"FoldedHeaderSpace", withHeaders(" X-Test: value")},
        {"FoldedHeaderTab", withHeaders("\tX-Test: value")},
        {"ControlInHeaderValue",
         withHeaders(
             "X-Test: a\x01"
             "b"
         )},
        {"DuplicateHost", withHeaders("hOsT: other")},
        {"ConflictingContentLengths", withHeaders("Content-Length: 1\r\ncontent-length: 2")},
        {"LengthBeforeTransferEncoding", withHeaders("Content-Length: 1\r\nTransfer-Encoding: chunked")},
        {"TransferEncodingBeforeLength", withHeaders("Transfer-Encoding: chunked\r\nContent-Length: 1")},

        // Only one Host
        {"TabInHost", GET + "Host: localh\tost\r\n\r\n"},
        {"SpaceInHost", GET + "Host: local host\r\n\r\n"},
        {"NonNumericHostPort", GET + "Host: localhost:abc\r\n\r\n"},

        // Content-Length.
        {"EmptyContentLength", withHeaders("Content-Length:")},
        {"NegativeContentLength", withHeaders("Content-Length: -1")},
        {"PositiveSignContentLength", withHeaders("Content-Length: +5")},
        {"LettersContentLength", withHeaders("Content-Length: abc")},
        {"TrailingTextContentLength", withHeaders("Content-Length: 5x")},
        {"ListContentLength", withHeaders("Content-Length: 1, 1")},
        {"OverflowContentLength", withHeaders("Content-Length: 999999999999999999999999999999999999")},

        // Chunked.
        {"InvalidHexChunkSize", CHUNKED + "Z\r\n"},
        {"NegativeChunkSize", CHUNKED + "-1\r\n"},
        {"EmptyChunkSize", CHUNKED + "\r\n"},
        {"InvalidChunkDataDelimiter", CHUNKED + "1\r\naXX"},
        {"InvalidFinalChunkDelimiter", CHUNKED + "0\r\nXX"},

        // unsupported transfer codings.
        {"UnsupportedGzip", POST + "Transfer-Encoding: gzip\r\n\r\n", HttpStatus::NotImplemented},
        {"UnsupportedGzipThenChunked", POST + "Transfer-Encoding: gzip, chunked\r\n\r\n", HttpStatus::NotImplemented},

        // Exceeding limits with and without a separator
        {"StartLineTooLong", startLine(LINE_LIMIT + 1) + "\r\nHost: localhost\r\n\r\n", HttpStatus::UriTooLong},
        {"UnterminatedStartLineTooLong", startLine(LINE_LIMIT + 1), HttpStatus::UriTooLong},
        {"HeadersTooLarge",
         GET + headersBlock(HEADERS_LIMIT + 1) + "\r\n\r\n",
         HttpStatus::RequestHeaderFieldsTooLarge},
        {"UnterminatedHeadersTooLarge", GET + headersBlock(HEADERS_LIMIT + 1), HttpStatus::RequestHeaderFieldsTooLarge},
        {"ContentLengthTooLarge", withLength(BODY_LIMIT + 1), HttpStatus::PayloadTooLarge},
        {"ChunkBodyTooLarge", CHUNKED + "a00001\r\n", HttpStatus::PayloadTooLarge},
        {"ChunkSizeLineTooLong", CHUNKED + chunkLine(CHUNK_LINE_LIMIT + 1) + "\r\n"},
        {"UnterminatedChunkSizeLineTooLong", CHUNKED + chunkLine(CHUNK_LINE_LIMIT + 1)},
    };

    INSTANTIATE_TEST_SUITE_P(
        Cases, HttpParserErrorTest, testing::ValuesIn(ERROR_CASES), [](const testing::TestParamInfo<ErrorCase>& info) {
            return std::string(info.param.name);
        }
    );

    //Receipt by chunks

    struct FragmentCase {
        const char* name;
        std::string raw;
        std::string body;
    };

    class HttpParserFragmentTest : public testing::TestWithParam<FragmentCase> {};

    TEST_P(HttpParserFragmentTest, EveryTwoPartSplit) {
        const auto& test = GetParam();

        for (std::size_t split = 1; split < test.raw.size(); ++split) {
            SCOPED_TRACE("split at " + std::to_string(split));

            HttpParser parser;

            const auto first = result(parser, std::string_view(test.raw).substr(0, split));
            ASSERT_TRUE(std::holds_alternative<NeedMoreData>(first));

            expectBody(result(parser, std::string_view(test.raw).substr(split)), test.body);
        }
    }

    // Verify that the parser correctly processes a request one byte at a time.

    TEST_P(HttpParserFragmentTest, ByteByByte) {
        const auto& test = GetParam();
        HttpParser parser;

        for (std::size_t i = 0; i < test.raw.size(); ++i) {
            SCOPED_TRACE("byte " + std::to_string(i));

            const auto byteResult = result(parser, std::string_view(test.raw).substr(i, 1));

            if (i + 1 == test.raw.size()) {
                expectBody(byteResult, test.body);
            } else {
                ASSERT_TRUE(std::holds_alternative<NeedMoreData>(byteResult));
            }
        }
    }

    const FragmentCase FRAGMENT_CASES[] = {
        {"Get", GET + "Host: localhost\r\n\r\n", ""},
        {"ContentLength", withLength(5) + "hello", "hello"},
        {"Chunked", CHUNKED + "4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n", "Wikipedia"},
    };

    INSTANTIATE_TEST_SUITE_P(
        Requests,
        HttpParserFragmentTest,
        testing::ValuesIn(FRAGMENT_CASES),
        [](const testing::TestParamInfo<FragmentCase>& info) { return std::string(info.param.name); }
    );

    // Content-Length

    TEST(HttpParserTest, ZeroContentLength) {
        expectBody(parse(withLength(0)), "");
    }

    TEST(HttpParserTest, BinaryBody) {
        const std::string body("a\0b", 3);

        expectBody(parse(withLength(body.size()) + body), body);
    }

    TEST(HttpParserTest, WaitsForRemainingBody) {
        HttpParser parser;

        const auto first = result(parser, withLength(5) + "hel");
        ASSERT_TRUE(std::holds_alternative<NeedMoreData>(first));

        expectBody(result(parser, "lo"), "hello");
    }

    TEST(HttpParserTest, PreservesNextRequestAfterBody) {
        HttpParser parser;

        const auto first = result(parser, withLength(5) + "hello" + GET + "Host: localhost\r\n\r\n");

        const auto* firstRequest = std::get_if<Complete>(&first);
        ASSERT_NE(firstRequest, nullptr);

        EXPECT_EQ(firstRequest->request.method, HttpMethod::Post);
        EXPECT_EQ(firstRequest->request.body, "hello");

        parser.reset();

        const auto second = result(parser, "");
        const auto* secondRequest = std::get_if<Complete>(&second);
        ASSERT_NE(secondRequest, nullptr);

        EXPECT_EQ(secondRequest->request.method, HttpMethod::Get);
        EXPECT_EQ(secondRequest->request.path, "/");
        EXPECT_TRUE(secondRequest->request.body.empty());
    }


    TEST(HttpParserTest, EmptyChunkedBody) {
        expectBody(parse(CHUNKED + "0\r\n\r\n"), "");
    }

    TEST(HttpParserTest, ChunkExtensionsAndMultipleChunks) {
        expectBody(
            parse(
                CHUNKED +
                "A;name=value\r\n0123456789\r\n"
                "1\r\n!\r\n"
                "0\r\n\r\n"
            ),
            "0123456789!"
        );
    }

    TEST(HttpParserTest, WaitsForFinalChunkAndDelimiter) {
        HttpParser parser;

        const auto first = result(parser, CHUNKED + "1\r\na\r\n");
        ASSERT_TRUE(std::holds_alternative<NeedMoreData>(first));

        const auto second = result(parser, "0\r\n");
        ASSERT_TRUE(std::holds_alternative<NeedMoreData>(second));

        expectBody(result(parser, "\r\n"), "a");
    }

    // SizeBoundary

    TEST(HttpParserTest, StartLineSizeBoundary) {
        for (const auto size : {LINE_LIMIT - 1, LINE_LIMIT}) {
            SCOPED_TRACE(size);

            expectBody(parse(startLine(size) + "\r\nHost: localhost\r\n\r\n"), "");

            EXPECT_TRUE(std::holds_alternative<NeedMoreData>(parse(startLine(size))));
        }
    }

    TEST(HttpParserTest, HeadersSizeBoundary) {
        for (const auto size : {HEADERS_LIMIT - 1, HEADERS_LIMIT}) {
            SCOPED_TRACE(size);

            expectBody(parse(GET + headersBlock(size) + "\r\n\r\n"), "");

            EXPECT_TRUE(std::holds_alternative<NeedMoreData>(parse(GET + headersBlock(size))));
        }
    }

    TEST(HttpParserTest, ContentLengthBodySizeBoundary) {
        for (const auto size : {BODY_LIMIT - 1, BODY_LIMIT}) {
            SCOPED_TRACE(size);

            const std::string body(size, 'a');
            expectBody(parse(withLength(size) + body), body);
        }
    }

    TEST(HttpParserTest, ChunkedBodySizeBoundary) {
        const std::string body(BODY_LIMIT, 'a');

        // 0xa00000 == 10 МиБ.
        expectBody(parse(CHUNKED + "a00000\r\n" + body + "\r\n0\r\n\r\n"), body);
    }

    TEST(HttpParserTest, CumulativeChunkedBodyTooLarge) {
        const std::string body(BODY_LIMIT, 'a');

        // The first chunk takes up the entire limit, and the second adds another byte.
        expectStatus(parse(CHUNKED + "a00000\r\n" + body + "\r\n1\r\n"), HttpStatus::PayloadTooLarge);
    }

    TEST(HttpParserTest, ChunkSizeLineBoundary) {
        for (const auto size : {CHUNK_LINE_LIMIT - 1, CHUNK_LINE_LIMIT}) {
            SCOPED_TRACE(size);

            expectBody(parse(CHUNKED + chunkLine(size) + "\r\na\r\n0\r\n\r\n"), "a");

            EXPECT_TRUE(std::holds_alternative<NeedMoreData>(parse(CHUNKED + chunkLine(size))));
        }
    }

    // DelimiterAtLimit

    TEST(HttpParserTest, SplitStartLineDelimiterAtLimit) {
        HttpParser parser;

        const auto first = result(parser, startLine(LINE_LIMIT) + "\r");
        ASSERT_TRUE(std::holds_alternative<NeedMoreData>(first));

        expectBody(result(parser, "\nHost: localhost\r\n\r\n"), "");
    }

    class HttpParserHeaderDelimiterTest : public testing::TestWithParam<std::size_t> {};

    TEST_P(HttpParserHeaderDelimiterTest, SplitAtHeaderLimit) {
        const std::string delimiter = "\r\n\r\n";
        const std::size_t split = GetParam();

        HttpParser parser;

        const auto first = result(parser, GET + headersBlock(HEADERS_LIMIT) + delimiter.substr(0, split));
        ASSERT_TRUE(std::holds_alternative<NeedMoreData>(first));

        expectBody(result(parser, delimiter.substr(split)), "");
    }

    INSTANTIATE_TEST_SUITE_P(
        SplitPositions, HttpParserHeaderDelimiterTest, testing::Values(std::size_t{1}, std::size_t{2}, std::size_t{3})
    );

    TEST(HttpParserTest, SplitChunkDelimiterAtLimit) {
        HttpParser parser;

        const auto first = result(parser, CHUNKED + chunkLine(CHUNK_LINE_LIMIT) + "\r");
        ASSERT_TRUE(std::holds_alternative<NeedMoreData>(first));

        expectBody(result(parser, "\na\r\n0\r\n\r\n"), "a");
    }

    TEST(HttpParserTest, RejectsStartLineOverflowAroundPartialDelimiter) {
        HttpParser parser;
        const auto first = result(parser, startLine(LINE_LIMIT) + "\r");
        ASSERT_TRUE(std::holds_alternative<NeedMoreData>(first));

        expectStatus(result(parser, "X"), HttpStatus::UriTooLong);
        expectStatus(parse(startLine(LINE_LIMIT + 1) + "\r"), HttpStatus::UriTooLong);
    }

    TEST(HttpParserTest, RejectsHeadersOverflowAroundPartialDelimiter) {
        const std::string delimiter = "\r\n\r\n";
        for (std::size_t split = 1; split < delimiter.size(); ++split) {
            SCOPED_TRACE(split);
            HttpParser parser;
            const auto first = result(parser, GET + headersBlock(HEADERS_LIMIT) + delimiter.substr(0, split));
            ASSERT_TRUE(std::holds_alternative<NeedMoreData>(first));

            expectStatus(result(parser, "X"), HttpStatus::RequestHeaderFieldsTooLarge);
            expectStatus(
                parse(GET + headersBlock(HEADERS_LIMIT + 1) + delimiter.substr(0, split)),
                HttpStatus::RequestHeaderFieldsTooLarge
            );
        }
    }

    TEST(HttpParserTest, RejectsChunkSizeOverflowAroundPartialDelimiter) {
        HttpParser parser;
        const auto first = result(parser, CHUNKED + chunkLine(CHUNK_LINE_LIMIT) + "\r");
        ASSERT_TRUE(std::holds_alternative<NeedMoreData>(first));

        expectStatus(result(parser, "X"), HttpStatus::BadRequest);
        expectStatus(parse(CHUNKED + chunkLine(CHUNK_LINE_LIMIT + 1) + "\r"), HttpStatus::BadRequest);
    }

} // namespace
