#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "http/CgiResponseParser.hpp"

namespace {

    std::optional<HttpResponse> parse(const std::string& output) {
        return CgiResponseParser::parse(output);
    }

    TEST(CgiResponseParserTest, DefaultsTo200) {
        const auto response = parse("Content-Type: text/plain\n\nhello");

        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status, HttpStatus::OK);
        EXPECT_EQ(response->body, "hello");
    }

    TEST(CgiResponseParserTest, LocationWithoutStatusIs302) {
        const auto response = parse("Location: /next\n\n");

        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status, HttpStatus::Found);
    }

    TEST(CgiResponseParserTest, PassesThroughAnyFinalStatus) {
        for (const int code : {200, 206, 401, 404, 418, 422, 429, 503, 599}) {
            SCOPED_TRACE(code);
            const auto response = parse("Status: " + std::to_string(code) + " Whatever\n\n");

            ASSERT_TRUE(response.has_value());
            EXPECT_EQ(static_cast<int>(response->status), code);
        }
    }

    TEST(CgiResponseParserTest, AcceptsStatusWithoutReason) {
        const auto response = parse("Status: 404\n\n");

        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status, HttpStatus::NotFound);
    }

    TEST(CgiResponseParserTest, RejectsInvalidStatus) {
        for (const char* status : {"100", "101", "199", "600", "999", "2001", "20x", "abc", "42"}) {
            SCOPED_TRACE(status);

            EXPECT_FALSE(parse(std::string("Status: ") + status + "\n\n").has_value());
        }
    }

} // namespace
