#include <cctype>
#include <cstddef>
#include <regex>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "http/HttpSerializer.hpp"

namespace {

    class HttpSerializerTest : public ::testing::Test {
    protected:
        HttpResponse response_{};

        void SetUp() override { response_.status = HttpStatus::OK; }

        static std::string lowercase(std::string value) {
            for (char& character : value) {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }

            return value;
        }

        std::vector<std::string> headerValues(const std::string& serialized, const std::string& name) const {
            const auto headerEnd = serialized.find("\r\n\r\n");

            if (headerEnd == std::string::npos) {
                ADD_FAILURE() << "Missing header/body separator";
                return {};
            }

            std::vector<std::string> values;
            const std::string expectedName = lowercase(name);
            std::size_t position = serialized.find("\r\n") + 2;

            while (position < headerEnd) {
                const auto lineEnd = serialized.find("\r\n", position);
                const auto line = serialized.substr(position, lineEnd - position);
                const auto separator = line.find(": ");

                if (separator == std::string::npos) {
                    ADD_FAILURE() << "Invalid header format: " << line;
                } else if (lowercase(line.substr(0, separator)) == expectedName) {
                    values.push_back(line.substr(separator + 2));
                }

                position = lineEnd + 2;
            }

            return values;
        }

        void expectHeader(const std::string& serialized, const std::string& name, const std::string& expected) const {
            const auto values = headerValues(serialized, name);

            ASSERT_EQ(values.size(), 1u) << name;
            EXPECT_EQ(values.front(), expected) << name;
        }

        void expectBody(const std::string& serialized, const std::string& expected) const {
            const auto separator = serialized.find("\r\n\r\n");

            ASSERT_NE(separator, std::string::npos);
            EXPECT_EQ(serialized.substr(separator + 4), expected);
        }
    };


    struct StatusLineCase {
        const char* name;
        HttpStatus status;
        const char* expected;
    };

    class HttpSerializerStatusLineTest : public HttpSerializerTest,
                                         public ::testing::WithParamInterface<StatusLineCase> {};

    TEST_P(HttpSerializerStatusLineTest, WritesHttpVersionCodeAndReasonPhrase) {
        const auto& test = GetParam();
        response_.status = test.status;

        const auto serialized = HttpSerializer::serialize(response_, {});
        const auto lineEnd = serialized.find("\r\n");

        ASSERT_NE(lineEnd, std::string::npos);
        EXPECT_EQ(serialized.substr(0, lineEnd + 2), test.expected);
    }

    INSTANTIATE_TEST_SUITE_P(
        StatusLines,
        HttpSerializerStatusLineTest,
        ::testing::Values(
            StatusLineCase{"OK200", HttpStatus::OK, "HTTP/1.1 200 OK\r\n"},
            StatusLineCase{"Created201", HttpStatus::Created, "HTTP/1.1 201 Created\r\n"},
            StatusLineCase{"NoContent204", HttpStatus::NoContent, "HTTP/1.1 204 No Content\r\n"},
            StatusLineCase{"MovedPermanently301", HttpStatus::MovedPermanently, "HTTP/1.1 301 Moved Permanently\r\n"},
            StatusLineCase{"NotModified304", HttpStatus::NotModified, "HTTP/1.1 304 Not Modified\r\n"},
            StatusLineCase{"BadRequest400", HttpStatus::BadRequest, "HTTP/1.1 400 Bad Request\r\n"},
            StatusLineCase{"Forbidden403", HttpStatus::Forbidden, "HTTP/1.1 403 Forbidden\r\n"},
            StatusLineCase{"NotFound404", HttpStatus::NotFound, "HTTP/1.1 404 Not Found\r\n"},
            StatusLineCase{
                "InternalServerError500", HttpStatus::InternalServerError, "HTTP/1.1 500 Internal Server Error\r\n"
            }
        ),
        [](const ::testing::TestParamInfo<StatusLineCase>& info) { return std::string(info.param.name); }
    );


    TEST_F(HttpSerializerTest, PreservesResponseHeadersAndTheirValues) {
        response_.headers.set("Content-Type", "text/plain; charset=utf-8");
        response_.headers.set("Connection", "close");
        response_.headers.set("X-Test", "value: with spaces");
        response_.headers.set("X-Empty", "");
        response_.body = "Hello";

        const auto serialized = HttpSerializer::serialize(response_, {});

        expectHeader(serialized, "Content-Type", "text/plain; charset=utf-8");
        expectHeader(serialized, "Connection", "close");
        expectHeader(serialized, "X-Test", "value: with spaces");
        expectHeader(serialized, "X-Empty", "");
        expectBody(serialized, "Hello");

        EXPECT_NE(serialized.find("\r\nContent-Type: text/plain; charset=utf-8\r\n"), std::string::npos);
        EXPECT_NE(serialized.find("\r\nX-Empty: \r\n"), std::string::npos);
    }

    TEST_F(HttpSerializerTest, UsesCRLFAndSeparatesHeadersFromBody) {
        response_.headers.set("Content-Type", "text/plain");
        response_.body = "body";

        const auto serialized = HttpSerializer::serialize(response_, {});
        const auto separator = serialized.find("\r\n\r\n");

        ASSERT_NE(separator, std::string::npos);
        EXPECT_EQ(serialized.substr(separator + 4), "body");

        for (std::size_t i = 0; i < separator + 4; ++i) {
            if (serialized[i] == '\n') {
                ASSERT_GT(i, 0u);
                EXPECT_EQ(serialized[i - 1], '\r');
            }

            if (serialized[i] == '\r') {
                ASSERT_LT(i + 1, serialized.size());
                EXPECT_EQ(serialized[i + 1], '\n');
            }
        }
    }

    TEST_F(HttpSerializerTest, AddsDefaultServerHeader) {
        const auto serialized = HttpSerializer::serialize(response_, {});

        expectHeader(serialized, "Server", "webserv");
    }

    TEST_F(HttpSerializerTest, PreservesCustomServerHeaderRegardlessOfCase) {
        for (const std::string name : {"Server", "server", "sErVeR"}) {
            SCOPED_TRACE(name);

            auto response = response_;
            response.headers.set(name, "custom-server");

            const auto serialized = HttpSerializer::serialize(response, {});

            expectHeader(serialized, "Server", "custom-server");
        }
    }

    TEST_F(HttpSerializerTest, GeneratesDateInsteadOfUsingProvidedValue) {
        response_.headers.set("dAtE", "user-supplied-date");

        const auto serialized = HttpSerializer::serialize(response_, {});
        const auto dates = headerValues(serialized, "Date");

        ASSERT_EQ(dates.size(), 1u);

        const std::regex httpDatePattern(
            "(Sun|Mon|Tue|Wed|Thu|Fri|Sat), "
            "[0-9]{2} "
            "(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) "
            "[0-9]{4} "
            "[0-9]{2}:[0-9]{2}:[0-9]{2} GMT"
        );

        EXPECT_TRUE(std::regex_match(dates.front(), httpDatePattern)) << dates.front();
    }

    TEST_F(HttpSerializerTest, RecalculatesContentLengthRegardlessOfHeaderCase) {
        for (const std::string name : {"Content-Length", "content-length", "cOnTeNt-LeNgTh"}) {
            SCOPED_TRACE(name);

            auto response = response_;
            response.body = "Hello";
            response.headers.set(name, "999");

            const auto serialized = HttpSerializer::serialize(response, {});

            expectHeader(serialized, "Content-Length", "5");
            expectBody(serialized, "Hello");
        }
    }

    TEST_F(HttpSerializerTest, OmitsTransferEncodingRegardlessOfHeaderCase) {
        for (const std::string name : {"Transfer-Encoding", "transfer-encoding", "tRaNsFeR-EnCoDiNg"}) {
            SCOPED_TRACE(name);

            auto response = response_;
            response.body = "Hello";
            response.headers.set(name, "chunked");

            const auto serialized = HttpSerializer::serialize(response, {});

            EXPECT_TRUE(headerValues(serialized, "Transfer-Encoding").empty());
            expectHeader(serialized, "Content-Length", "5");
            expectBody(serialized, "Hello");
        }
    }


    struct BodyCase {
        const char* name;
        std::string body;
        std::size_t expectedBytes;
    };

    class HttpSerializerBodyTest : public HttpSerializerTest, public ::testing::WithParamInterface<BodyCase> {};

    TEST_P(HttpSerializerBodyTest, WritesByteLengthAndPreservesBody) {
        const auto& test = GetParam();
        response_.body = test.body;

        const auto serialized = HttpSerializer::serialize(response_, {});

        expectHeader(serialized, "Content-Length", std::to_string(test.expectedBytes));
        expectBody(serialized, test.body);

        const auto separator = serialized.find("\r\n\r\n");
        ASSERT_NE(separator, std::string::npos);
        EXPECT_EQ(serialized.size() - separator - 4, test.expectedBytes);
    }

    INSTANTIATE_TEST_SUITE_P(
        Bodies,
        HttpSerializerBodyTest,
        ::testing::Values(
            BodyCase{"Empty", "", 0},
            BodyCase{"Ascii", "Hello", 5},

            BodyCase{"Utf8Cyrillic", "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82", 12},

            BodyCase{"Utf8Emoji", "\xF0\x9F\x98\x80", 4},
            BodyCase{"EmbeddedNullBytes", std::string("A\0B\0C", 5), 5},
            BodyCase{"BinaryBytes", std::string("\x00\x01\x7f\x80\xff", 5), 5},
            BodyCase{"ContainsHeaderSeparator", "a\r\n\r\nb", 6},
            BodyCase{"LargeBody", std::string(65537, 'x'), 65537}
        ),
        [](const ::testing::TestParamInfo<BodyCase>& info) { return std::string(info.param.name); }
    );

    TEST_F(HttpSerializerTest, HeadersOnlyPreservesLengthButOmitsBody) {
        response_.body = std::string("A\0B", 3);
        response_.headers.set("Content-Type", "application/octet-stream");

        const auto serialized = HttpSerializer::serialize(response_, {.headersOnly = true});

        expectHeader(serialized, "Content-Length", "3");
        expectHeader(serialized, "Content-Type", "application/octet-stream");
        expectBody(serialized, "");
    }


    TEST_F(HttpSerializerTest, NoContentOmitsBodyAndFramingHeaders) {
        response_.status = HttpStatus::NoContent;
        response_.body = std::string("A\0B", 3) + "\r\n\r\nhidden body";
        response_.headers.set("cOnTeNt-LeNgTh", "999");
        response_.headers.set("tRaNsFeR-EnCoDiNg", "chunked");
        response_.headers.set("X-Test", "preserved");

        for (const bool headersOnly : {false, true}) {
            SCOPED_TRACE(headersOnly);

            const auto serialized = HttpSerializer::serialize(response_, {.headersOnly = headersOnly});

            expectBody(serialized, "");
            expectHeader(serialized, "X-Test", "preserved");

            EXPECT_TRUE(headerValues(serialized, "Content-Length").empty());
            EXPECT_TRUE(headerValues(serialized, "Transfer-Encoding").empty());
        }
    }

    TEST_F(HttpSerializerTest, EmptyNoContentDoesNotAddZeroContentLength) {
        response_.status = HttpStatus::NoContent;
        response_.body.clear();

        const auto serialized = HttpSerializer::serialize(response_, {});

        expectBody(serialized, "");
        EXPECT_TRUE(headerValues(serialized, "Content-Length").empty());
        EXPECT_TRUE(headerValues(serialized, "Transfer-Encoding").empty());
    }

} // namespace
