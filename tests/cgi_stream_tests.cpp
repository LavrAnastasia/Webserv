#include <algorithm>
#include <chrono>
#include <poll.h>
#include <string>
#include <variant>

#include <gtest/gtest.h>

#include "cgi/CgiRegistry.hpp"
#include "http/CgiResponse.hpp"
#include "http/CgiResponseParser.hpp"
#include "includes/Files.hpp"
#include "includes/TempDirectory.hpp"

namespace {
    TEST(CgiStreamTest, AcceptsEveryHeaderBoundaryAndPreservesBodyBytes) {
        for (const std::string delimiter : {"\n", "\r\n"}) {
            const std::string header = "Content-Type: application/octet-stream" + delimiter + delimiter;
            const std::string body("a\0b\xff", 4);
            for (std::size_t boundary = 0; boundary <= header.size(); ++boundary) {
                CgiResponse stream(false, false);
                ASSERT_TRUE(stream.append(std::string_view(header).substr(0, boundary)));
                ASSERT_TRUE(stream.append(header.substr(boundary) + body));
                ASSERT_TRUE(stream.finish());
                const std::string output(stream.data());
                const auto start = output.find("\r\n\r\n");
                ASSERT_NE(start, std::string::npos);
                EXPECT_NE(output.find("Transfer-Encoding: chunked\r\n"), std::string::npos);
                EXPECT_EQ(output.substr(start + 4), "4\r\n" + body + "\r\n0\r\n\r\n");
                EXPECT_FALSE(stream.done());
                stream.consume(output.size());
                EXPECT_TRUE(stream.done());
            }
        }
    }

    TEST(CgiStreamTest, BoundsBufferAndResumesAfterPartialConsumption) {
        CgiResponse stream(false, false);
        ASSERT_TRUE(stream.append("Content-Type: text/plain\n\n"));
        std::size_t appended = 0;
        while (stream.capacity() != 0) {
            const std::string bytes(stream.capacity(), 'a');
            ASSERT_TRUE(stream.append(bytes));
            appended += bytes.size();
            ASSERT_LE(stream.data().size(), 64u * 1024u);
        }
        EXPECT_GT(appended, 60000u);
        EXPECT_FALSE(stream.append("x"));
        const std::string remaining(stream.data().substr(1000));
        stream.consume(1000);
        ASSERT_GT(stream.capacity(), 0u);
        ASSERT_TRUE(stream.append("next"));
        EXPECT_EQ(stream.data(), remaining + "4\r\nnext\r\n");
        ASSERT_TRUE(stream.finish());
        EXPECT_TRUE(stream.data().ends_with("0\r\n\r\n"));
        EXPECT_LE(stream.data().size(), 64u * 1024u);
    }

    TEST(CgiStreamTest, EmptyBufferIsNotEofAndDoesNotGenerateAnEmptyChunk) {
        CgiResponse stream(false, false);
        ASSERT_TRUE(stream.append("Content-Type: text/plain\n\nfirst"));
        stream.consume(stream.data().size());
        EXPECT_TRUE(stream.started());
        EXPECT_TRUE(stream.data().empty());
        EXPECT_FALSE(stream.done());
        ASSERT_TRUE(stream.append(""));
        EXPECT_TRUE(stream.data().empty());
        ASSERT_TRUE(stream.append("second"));
        EXPECT_EQ(stream.data(), "6\r\nsecond\r\n");
        ASSERT_TRUE(stream.finish());
        EXPECT_EQ(stream.data(), "6\r\nsecond\r\n0\r\n\r\n");
    }

    TEST(CgiStreamTest, EnforcesContentLengthWithoutChunkMarkers) {
        CgiResponse stream(false, false);
        ASSERT_TRUE(stream.append("Content-Type: text/plain\nContent-Length: 6\n\nabc"));
        const std::string first(stream.data());
        EXPECT_NE(first.find("Content-Length: 6\r\n"), std::string::npos);
        EXPECT_EQ(first.find("Transfer-Encoding:"), std::string::npos);
        stream.consume(first.size());
        ASSERT_TRUE(stream.append("def"));
        EXPECT_EQ(stream.data(), "def");
        EXPECT_FALSE(stream.append("extra"));
        ASSERT_TRUE(stream.finish());
        EXPECT_EQ(stream.data(), "def");

        CgiResponse truncated(false, false);
        ASSERT_TRUE(truncated.append("Content-Length: 4\n\nabc"));
        EXPECT_FALSE(truncated.finish());
        EXPECT_FALSE(truncated.data().ends_with("0\r\n\r\n"));
    }

    TEST(CgiStreamTest, SuppressesHeadAndBodylessStatuses) {
        for (const int status : {200, 204, 304}) {
            for (const bool head : {false, true}) {
                if (status == 200 && !head) {
                    continue;
                }
                CgiResponse stream(false, head);
                ASSERT_TRUE(stream.append("Status: " + std::to_string(status) + "\nContent-Length: 123\n\nignored"));
                const std::string output(stream.data());
                EXPECT_TRUE(output.ends_with("\r\n\r\n"));
                EXPECT_EQ(output.find("Transfer-Encoding:"), std::string::npos);
                EXPECT_EQ(output.find("Content-Length:") != std::string::npos, status == 200);
                stream.consume(output.size());
                ASSERT_TRUE(stream.append("more ignored bytes"));
                ASSERT_TRUE(stream.finish());
                EXPECT_TRUE(stream.done());
                EXPECT_TRUE(stream.data().empty());
            }
        }
    }

    TEST(CgiStreamTest, RejectsMissingInvalidAndExcessiveHeaders) {
        for (const std::string headers :
             {"\n",
              "broken\n\n",
              "Content-Length: -1\n\n",
              "Content-Length: +1\n\n",
              "Content-Length: 1x\n\n",
              "Content-Length: \n\n",
              "Content-Length: 1\nContent-Length: 1\n\n",
              "Content-Length: 9999999999999999999999999999999\n\n",
              "Transfer-Encoding: chunked\n\n"}) {
            CgiResponse stream(false, false);
            EXPECT_FALSE(stream.append(headers)) << headers;
        }
        CgiResponse missing(false, false);
        EXPECT_FALSE(missing.finish());
        CgiResponse incomplete(false, false);
        ASSERT_TRUE(incomplete.append("Content-Type: text/plain\n"));
        EXPECT_FALSE(incomplete.finish());
        CgiResponse excessive(false, false);
        EXPECT_FALSE(excessive.append(std::string(excessive.capacity(), 'x')));
    }

    TEST(CgiStreamTest, HeaderParserStopsBeforeBodyAndReportsConsumedBytes) {
        const std::string headers = "Content-Type: text/plain\nContent-Length: 100000\n\n";
        auto result = CgiResponseParser::parseHeaders(headers + std::string(100000, 'a'));
        const auto* head = std::get_if<CgiResponseParser::Head>(&result);
        ASSERT_NE(head, nullptr);
        EXPECT_EQ(head->consumed, headers.size());
        ASSERT_TRUE(head->length);
        EXPECT_EQ(*head->length, 100000u);
        EXPECT_TRUE(head->response.body.done());
    }

    TEST(CgiRegistryTest, MeasuresInactivityAndSuspendsTimeoutDuringBackpressure) {
        using namespace std::chrono_literals;
        using Clock = std::chrono::steady_clock;
        TempDirectory temp("webserv-cgi-registry");
        const auto script = temp.path() / "empty.sh";
        Files::write(script, "exit 0\n");
        auto process =
            CgiProcess::launch(CgiRequest{.interpreter = "/bin/sh", .script = script, .env = {}, .body = {}});
        ASSERT_TRUE(process);
        CgiRegistry registry;
        registry.add(123, std::move(*process));
        const auto launched = Clock::now();
        EXPECT_EQ(registry.expired(1s, launched + 1s), std::vector<int>{123});
        ::poll(nullptr, 0, 20);
        registry.touch(123);
        EXPECT_TRUE(registry.expired(1s, launched + 1s).empty());
        registry.pause(123, true);
        EXPECT_TRUE(registry.expired(1s, Clock::now() + 1h).empty());
        registry.pause(123, false);
        EXPECT_EQ(registry.expired(1s, Clock::now() + 2s), std::vector<int>{123});
    }
} // namespace
