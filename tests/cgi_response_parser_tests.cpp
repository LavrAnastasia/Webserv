#include <string>
#include <variant>

#include <gtest/gtest.h>

#include "../src/http/src/CgiResponseParser.hpp"

namespace {

    TEST(CgiResponseParserTest, DefaultsTo200) {
        auto result = CgiResponseParser::parseHeaders("Content-Type: text/plain\n\nhello");
        const auto* headers = std::get_if<CgiResponseParser::Headers>(&result);

        ASSERT_NE(headers, nullptr);
        EXPECT_EQ(headers->response.status, HttpStatus::OK);
        EXPECT_EQ(headers->consumed, std::string("Content-Type: text/plain\n\n").size());
        EXPECT_TRUE(headers->response.body.done());
    }

    TEST(CgiResponseParserTest, LocationWithoutStatusIs302) {
        const auto result = CgiResponseParser::parseHeaders("Location: /next\n\n");
        const auto* headers = std::get_if<CgiResponseParser::Headers>(&result);

        ASSERT_NE(headers, nullptr);
        EXPECT_EQ(headers->response.status, HttpStatus::Found);
    }

    TEST(CgiResponseParserTest, PassesThroughAnyFinalStatus) {
        for (const int code : {200, 206, 401, 404, 418, 422, 429, 503, 599}) {
            SCOPED_TRACE(code);
            const auto result = CgiResponseParser::parseHeaders("Status: " + std::to_string(code) + " Whatever\n\n");
            const auto* headers = std::get_if<CgiResponseParser::Headers>(&result);

            ASSERT_NE(headers, nullptr);
            EXPECT_EQ(static_cast<int>(headers->response.status), code);
        }
    }

    TEST(CgiResponseParserTest, AcceptsStatusWithoutReason) {
        const auto result = CgiResponseParser::parseHeaders("Status: 404\n\n");
        const auto* headers = std::get_if<CgiResponseParser::Headers>(&result);

        ASSERT_NE(headers, nullptr);
        EXPECT_EQ(headers->response.status, HttpStatus::NotFound);
    }

    TEST(CgiResponseParserTest, RejectsInvalidStatus) {
        for (const char* status : {"100", "101", "199", "600", "999", "2001", "20x", "abc", "42"}) {
            SCOPED_TRACE(status);

            EXPECT_TRUE(
                std::holds_alternative<CgiResponseParser::Invalid>(
                    CgiResponseParser::parseHeaders(std::string("Status: ") + status + "\n\n")
                )
            );
        }
    }

    TEST(CgiResponseParserTest, WaitsForEveryIncompleteHeaderPrefix) {
        const std::string output = "Content-Type: text/plain\r\nContent-Length: 5\r\n\r\n";

        for (std::size_t size = 0; size < output.size(); ++size) {
            SCOPED_TRACE(size);
            const auto result = CgiResponseParser::parseHeaders(std::string_view(output).substr(0, size));

            EXPECT_TRUE(std::holds_alternative<CgiResponseParser::Incomplete>(result));
        }

        const auto result = CgiResponseParser::parseHeaders(output);
        const auto* headers = std::get_if<CgiResponseParser::Headers>(&result);

        ASSERT_NE(headers, nullptr);
        EXPECT_EQ(headers->consumed, output.size());
        ASSERT_TRUE(headers->length.has_value());
        EXPECT_EQ(*headers->length, 5u);
        EXPECT_EQ(headers->response.headers.get("Content-Length"), "5");
    }

    TEST(CgiResponseParserTest, RejectsEmptyOrMalformedHeaders) {
        for (const char* output : {"\r\n", "\n", "broken\n\n", "Bad Header: value\n\n", " X: value\n\n"}) {
            SCOPED_TRACE(output);

            EXPECT_TRUE(std::holds_alternative<CgiResponseParser::Invalid>(CgiResponseParser::parseHeaders(output)));
        }
    }

    TEST(CgiResponseParserTest, RejectsInvalidContentLength) {
        for (const char* value : {"", "-1", "+1", "1x", "1, 1", "18446744073709551616"}) {
            SCOPED_TRACE(value);
            const auto result = CgiResponseParser::parseHeaders(std::string("Content-Length: ") + value + "\n\n");

            EXPECT_TRUE(std::holds_alternative<CgiResponseParser::Invalid>(result));
        }
    }

    TEST(CgiResponseParserTest, AcceptsZeroContentLength) {
        const auto result = CgiResponseParser::parseHeaders("Content-Length: 0\n\n");
        const auto* headers = std::get_if<CgiResponseParser::Headers>(&result);

        ASSERT_NE(headers, nullptr);
        ASSERT_TRUE(headers->length.has_value());
        EXPECT_EQ(*headers->length, 0u);
    }

    TEST(CgiResponseParserTest, RejectsDuplicateLengthAndTransferEncoding) {
        for (const char* output :
             {"Content-Length: 1\ncontent-length: 1\n\n",
              "Transfer-Encoding: chunked\n\n",
              "Transfer-Encoding: identity\n\n",
              "Content-Length: 1\nTransfer-Encoding: chunked\n\n"}) {
            SCOPED_TRACE(output);

            EXPECT_TRUE(std::holds_alternative<CgiResponseParser::Invalid>(CgiResponseParser::parseHeaders(output)));
        }
    }

    TEST(CgiResponseParserTest, StripsHopHeadersAndConnectionOptions) {
        const auto result = CgiResponseParser::parseHeaders(
            "Content-Type: text/plain\n"
            "Connection: keep-alive, X-Hop\n"
            "Connection: Upgrade\n"
            "Keep-Alive: timeout=10\n"
            "Proxy-Connection: close\n"
            "TE: trailers\n"
            "Trailer: X-Checksum\n"
            "Upgrade: other\n"
            "X-Hop: private\n"
            "X-Public: visible\n\n"
        );
        const auto* headers = std::get_if<CgiResponseParser::Headers>(&result);

        ASSERT_NE(headers, nullptr);
        for (const char* name : {"Connection", "Keep-Alive", "Proxy-Connection", "TE", "Trailer", "Upgrade", "X-Hop"}) {
            EXPECT_FALSE(headers->response.headers.has(name)) << name;
        }
        EXPECT_EQ(headers->response.headers.get("X-Public"), "visible");
    }

    TEST(CgiResponseParserTest, DoesNotFrameWithRemovedConnectionOption) {
        const auto result = CgiResponseParser::parseHeaders("Connection: Content-Length\nContent-Length: 5\n\n");
        const auto* headers = std::get_if<CgiResponseParser::Headers>(&result);

        ASSERT_NE(headers, nullptr);
        EXPECT_FALSE(headers->response.headers.has("Content-Length"));
        EXPECT_FALSE(headers->length.has_value());
    }

    TEST(CgiResponseParserTest, LimitsHeadersWithoutLimitingBody) {
        constexpr std::size_t limit = 16 * 1024;
        const std::string oversized = "X-Test: " + std::string(limit, 'x');

        EXPECT_TRUE(std::holds_alternative<CgiResponseParser::Invalid>(CgiResponseParser::parseHeaders(oversized)));
        EXPECT_TRUE(
            std::holds_alternative<CgiResponseParser::Invalid>(CgiResponseParser::parseHeaders(oversized + "\n\n"))
        );

        const std::string output = "Content-Type: text/plain\n\n" + std::string(limit * 2, 'x');
        const auto result = CgiResponseParser::parseHeaders(output);
        const auto* headers = std::get_if<CgiResponseParser::Headers>(&result);

        ASSERT_NE(headers, nullptr);
        EXPECT_EQ(output.size() - headers->consumed, limit * 2);
    }

} // namespace
