#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <gtest/gtest.h>

#include "config/ServerConfig.hpp"
#include "http/HttpSession.hpp"
#include "includes/Files.hpp"
#include "includes/Requests.hpp"
#include "includes/TempDirectory.hpp"

namespace {
    namespace fs = std::filesystem;

    using namespace std::chrono_literals;

    constexpr std::size_t kChunkSize = 64 * 1024;

    std::chrono::steady_clock::time_point fakeNow;

    std::chrono::steady_clock::time_point readFakeNow() {
        return fakeNow;
    }

    class HttpSessionTest : public testing::Test {
    protected:
        TempDirectory temp_{"webserv-session"};
        fs::path root_;
        ServerConfig server_{};
        std::unique_ptr<HttpSession> session_;
        HttpSession::Action end_;

        void SetUp() override {
            root_ = temp_.path() / "root";
            Files::write(root_ / "index.html", "hello");
            Files::write(root_ / "cgi/app.py", "print()");

            LocationConfig site{};
            site.path = "/";
            site.allowedMethods = {HttpMethod::Get, HttpMethod::Head};

            LocationConfig cgi{};
            cgi.path = "/cgi";
            cgi.allowedMethods = {HttpMethod::Get, HttpMethod::Head};
            cgi.cgi.emplace(".py", CgiConfig{".py", fs::path("/usr/bin/python3")});

            server_.root = root_;
            server_.locations = {site, cgi};
            fakeNow = std::chrono::steady_clock::time_point{};
            session_ = std::make_unique<HttpSession>(server_, ConnectionInfo{"10.0.0.7", 8080}, readFakeNow);
        }

        std::string deliver(HttpSession::Action action) {
            std::string text;
            int writes = 0;

            while (true) {
                const auto* write = std::get_if<HttpSession::Write>(&action);
                const auto* stream = std::get_if<HttpSession::Stream>(&action);

                if (write == nullptr && stream == nullptr) {
                    break;
                }

                const std::string& bytes = write != nullptr ? write->bytes : stream->bytes;

                if (bytes.empty() || ++writes > 1024) {
                    ADD_FAILURE() << "Response never ends or emits empty writes";
                    break;
                }

                text += bytes;
                action = session_->emit();
            }

            end_ = std::move(action);
            return text;
        }

        template <typename Action> bool ended() const { return std::holds_alternative<Action>(end_); }

        void startCgi(const std::string& request) {
            ASSERT_TRUE(std::holds_alternative<CgiRequest>(session_->receive(request)));
        }

        void receiveOutput(std::string_view output) {
            ASSERT_TRUE(std::holds_alternative<HttpSession::ReadOutput>(session_->receiveOutput(output)));
        }

        static bool closes(const std::string& response) {
            return response.find("Connection: close\r\n") != std::string::npos;
        }

        static std::string body(const std::string& response) { return response.substr(response.find("\r\n\r\n") + 4); }

        static std::string pattern(std::size_t size) {
            std::string bytes(size, '\0');

            for (std::size_t i = 0; i < size; ++i) {
                bytes[i] = static_cast<char>(i % 251);
            }

            return bytes;
        }
    };

    TEST_F(HttpSessionTest, WaitsForTheWholeRequestBeforeResponding) {
        EXPECT_TRUE(std::holds_alternative<HttpSession::Read>(session_->receive("GET / HTTP/1.1\r\nHo")));

        const std::string response = deliver(session_->receive("st: localhost\r\n\r\n"));

        EXPECT_TRUE(response.starts_with("HTTP/1.1 200 "));
        EXPECT_TRUE(ended<HttpSession::Read>());
    }

    TEST_F(HttpSessionTest, ClosesAfterAnsweringAMalformedRequest) {
        const std::string response = deliver(session_->receive("GARBAGE\r\n\r\n"));

        EXPECT_TRUE(response.starts_with("HTTP/1.1 400 "));
        EXPECT_TRUE(closes(response));
        EXPECT_TRUE(ended<HttpSession::Close>());
    }

    TEST_F(HttpSessionTest, AnswersAMalformedRequestWithTheErrorPageOfItsLocation) {
        const fs::path siteRoot = temp_.path() / "site";
        server_.locations.front().root = siteRoot;
        server_.errorPages = {{HttpStatus::BadRequest, "/errors/400.html"}};
        Files::write(root_ / "errors/400.html", "server 400");
        Files::write(siteRoot / "errors/400.html", "site 400");

        const std::string response = deliver(session_->receive("GARBAGE\r\n\r\n"));

        EXPECT_TRUE(response.starts_with("HTTP/1.1 400 "));
        EXPECT_EQ(body(response), "site 400");
    }

    TEST_F(HttpSessionTest, KeepsTheConnectionAndAnswersTheNextRequest) {
        const std::string first = deliver(session_->receive(Requests::rawGet("/")));

        EXPECT_TRUE(first.starts_with("HTTP/1.1 200 "));
        EXPECT_FALSE(closes(first));
        ASSERT_TRUE(ended<HttpSession::Read>());

        const std::string second = deliver(session_->receive(Requests::rawGet("/missing")));

        EXPECT_TRUE(second.starts_with("HTTP/1.1 404 "));
    }

    TEST_F(HttpSessionTest, ClosesWhenTheClientAsksForIt) {
        const std::string response = deliver(session_->receive(Requests::rawGet("/", "close")));

        EXPECT_TRUE(closes(response));
        EXPECT_TRUE(ended<HttpSession::Close>());
    }

    TEST_F(HttpSessionTest, HeadOmitsTheBodyOnlyForThatRequest) {
        const std::string head = deliver(session_->receive("HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n"));

        EXPECT_NE(head.find("Content-Length: 5\r\n"), std::string::npos);
        EXPECT_TRUE(body(head).empty());
        ASSERT_TRUE(ended<HttpSession::Read>());

        const std::string get = deliver(session_->receive(Requests::rawGet("/")));

        EXPECT_EQ(body(get), "hello");
    }

    TEST_F(HttpSessionTest, SendsTheErrorBodyForAMalformedRequestAfterHead) {
        deliver(session_->receive("HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n"));
        ASSERT_TRUE(ended<HttpSession::Read>());

        const std::string response = deliver(session_->receive("GARBAGE\r\n\r\n"));

        EXPECT_FALSE(body(response).empty());
    }

    TEST_F(HttpSessionTest, SendsHeadersWithTheFirstChunkAndTheRestInChunks) {
        for (const std::size_t size : {0u, 1u, 65536u, 65537u, 2 * 65536u + 10}) {
            SCOPED_TRACE(size);
            const std::string content = pattern(size);
            Files::write(root_ / "file.bin", content);

            HttpSession::Action action = session_->receive(Requests::rawGet("/file.bin"));
            auto* first = std::get_if<HttpSession::Write>(&action);

            ASSERT_NE(first, nullptr);
            EXPECT_EQ(body(first->bytes).size(), std::min(size, kChunkSize));

            const std::string response = deliver(std::move(action));

            EXPECT_EQ(body(response), content);
            EXPECT_TRUE(ended<HttpSession::Read>());
        }
    }

    TEST_F(HttpSessionTest, ClosesWhenTheFileShrinksDuringTheResponse) {
        Files::write(root_ / "file.bin", pattern(2 * kChunkSize));

        ASSERT_TRUE(std::holds_alternative<HttpSession::Write>(session_->receive(Requests::rawGet("/file.bin"))));
        fs::resize_file(root_ / "file.bin", 0);

        EXPECT_TRUE(std::holds_alternative<HttpSession::Close>(session_->emit()));
    }

    TEST_F(HttpSessionTest, ClosesAnIdleConnectionAfterTheKeepAliveTimeout) {
        fakeNow += 59s;
        EXPECT_FALSE(session_->expire().has_value());

        fakeNow += 1s;
        const auto action = session_->expire();

        ASSERT_TRUE(action.has_value());
        EXPECT_TRUE(std::holds_alternative<HttpSession::Close>(*action));
    }

    TEST_F(HttpSessionTest, CountsKeepAliveFromTheEndOfTheLastResponse) {
        HttpSession::Action response = session_->receive(Requests::rawGet("/"));
        fakeNow += 50s;
        deliver(std::move(response));
        ASSERT_TRUE(ended<HttpSession::Read>());

        fakeNow += 59s;
        EXPECT_FALSE(session_->expire().has_value());

        fakeNow += 1s;
        EXPECT_TRUE(session_->expire().has_value());
    }

    TEST_F(HttpSessionTest, ClosesWhenHeadersTrickleInForTooLong) {
        session_->receive("GET / HTTP/1.1\r\n");

        for (int second = 0; second < 19; ++second) {
            fakeNow += 1s;
            session_->receive("X");
            ASSERT_FALSE(session_->expire().has_value()) << second;
        }

        fakeNow += 1s;
        const auto action = session_->expire();

        ASSERT_TRUE(action.has_value());
        EXPECT_TRUE(std::holds_alternative<HttpSession::Close>(*action));
    }

    TEST_F(HttpSessionTest, StartsTheHeaderTimerForAPipelinedPartialRequest) {
        deliver(session_->receive(Requests::rawGet("/") + "GET / HT"));
        ASSERT_TRUE(ended<HttpSession::Read>());

        fakeNow += 20s;
        const auto action = session_->expire();

        ASSERT_TRUE(action.has_value());
        EXPECT_TRUE(std::holds_alternative<HttpSession::Close>(*action));
    }

    TEST_F(HttpSessionTest, AnswersTimeoutWhenTheBodyStopsArriving) {
        session_->receive("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\nabc");

        fakeNow += 59s;
        EXPECT_FALSE(session_->expire().has_value());

        fakeNow += 1s;
        auto action = session_->expire();

        ASSERT_TRUE(action.has_value());
        const std::string response = deliver(std::move(*action));

        EXPECT_TRUE(response.starts_with("HTTP/1.1 408 "));
        EXPECT_TRUE(closes(response));
        EXPECT_TRUE(ended<HttpSession::Close>());
    }

    TEST_F(HttpSessionTest, KeepsASlowButSteadyBody) {
        session_->receive("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\n");

        for (int byte = 0; byte < 3; ++byte) {
            fakeNow += 50s;
            session_->receive("a");
        }

        fakeNow += 55s;

        EXPECT_FALSE(session_->expire().has_value());
    }

    TEST_F(HttpSessionTest, IgnoresItsTimersWhileResponding) {
        ASSERT_TRUE(std::holds_alternative<HttpSession::Write>(session_->receive(Requests::rawGet("/"))));

        fakeNow += 1h;

        EXPECT_FALSE(session_->expire().has_value());
    }

    TEST_F(HttpSessionTest, AnswersBytesReceivedWhileRespondingAfterTheResponse) {
        HttpSession::Action first = session_->receive(Requests::rawGet("/"));

        ASSERT_TRUE(std::holds_alternative<HttpSession::Write>(first));
        EXPECT_TRUE(std::holds_alternative<HttpSession::Read>(session_->receive(Requests::rawGet("/missing"))));

        const std::string responses = deliver(std::move(first));

        EXPECT_LT(responses.find("HTTP/1.1 200 "), responses.find("HTTP/1.1 404 "));
        EXPECT_NE(responses.find("HTTP/1.1 404 "), std::string::npos);
        EXPECT_TRUE(ended<HttpSession::Read>());
    }

    TEST_F(HttpSessionTest, AnswersPipelinedRequestsInOrder) {
        const std::string responses = deliver(session_->receive(Requests::rawGet("/") + Requests::rawGet("/missing")));

        EXPECT_LT(responses.find("HTTP/1.1 200 "), responses.find("HTTP/1.1 404 "));
        EXPECT_NE(responses.find("HTTP/1.1 404 "), std::string::npos);
        EXPECT_TRUE(ended<HttpSession::Read>());
    }

    TEST_F(HttpSessionTest, IgnoresRequestsPipelinedAfterClose) {
        const std::string responses =
            deliver(session_->receive(Requests::rawGet("/", "close") + Requests::rawGet("/missing")));

        EXPECT_EQ(responses.find("HTTP/1.1 404 "), std::string::npos);
        EXPECT_TRUE(ended<HttpSession::Close>());
    }

    TEST_F(HttpSessionTest, WaitsForTheRestOfAPipelinedRequest) {
        const std::string second = Requests::rawGet("/missing");
        const std::string first = deliver(session_->receive(Requests::rawGet("/") + second.substr(0, 10)));

        EXPECT_EQ(first.find("HTTP/1.1 404 "), std::string::npos);
        ASSERT_TRUE(ended<HttpSession::Read>());

        const std::string rest = deliver(session_->receive(second.substr(10)));

        EXPECT_TRUE(rest.starts_with("HTTP/1.1 404 "));
    }

    TEST_F(HttpSessionTest, PassesTheClientAddressToCgi) {
        HttpSession::Action action = session_->receive(Requests::rawGet("/cgi/app.py"));
        auto* request = std::get_if<CgiRequest>(&action);

        ASSERT_NE(request, nullptr);
        EXPECT_NE(std::ranges::find(request->env, "REMOTE_ADDR=10.0.0.7"), request->env.end());
    }

    TEST_F(HttpSessionTest, StreamsHeadersAndBodyAcrossReads) {
        startCgi(Requests::rawGet("/cgi/app.py"));
        receiveOutput("Status: 201 Created\r\nContent-");
        receiveOutput("Type: text/plain\r\n\r");

        HttpSession::Action action = session_->receiveOutput("\nhel");
        ASSERT_TRUE(std::holds_alternative<HttpSession::Stream>(action));
        const std::string first = deliver(std::move(action));

        EXPECT_TRUE(first.starts_with("HTTP/1.1 201 "));
        EXPECT_NE(first.find("Transfer-Encoding: chunked\r\n"), std::string::npos);
        EXPECT_EQ(first.find("Content-Length:"), std::string::npos);
        EXPECT_EQ(body(first), "3\r\nhel\r\n");
        EXPECT_TRUE(ended<HttpSession::ReadOutput>());
        action = session_->receiveOutput("lo");
        ASSERT_TRUE(std::holds_alternative<HttpSession::Stream>(action));
        EXPECT_EQ(deliver(std::move(action)), "2\r\nlo\r\n");
        EXPECT_TRUE(ended<HttpSession::ReadOutput>());
        action = session_->endOutput();
        ASSERT_TRUE(std::holds_alternative<HttpSession::Write>(action));
        EXPECT_EQ(deliver(std::move(action)), "0\r\n\r\n");
        EXPECT_TRUE(ended<HttpSession::Read>());
    }

    TEST_F(HttpSessionTest, WaitsForOutputWhileCgiRuns) {
        startCgi(Requests::rawGet("/cgi/app.py"));

        EXPECT_TRUE(std::holds_alternative<HttpSession::ReadOutput>(session_->emit()));
        receiveOutput("Content-Type:");
        EXPECT_TRUE(std::holds_alternative<HttpSession::ReadOutput>(session_->emit()));
    }

    TEST_F(HttpSessionTest, PreservesContentLengthAndWaitsForEof) {
        startCgi(Requests::rawGet("/cgi/app.py"));
        const std::string first =
            deliver(session_->receiveOutput("Content-Type: text/plain\r\nContent-Length: 0005\r\n\r\nhel"));

        EXPECT_NE(first.find("Content-Length: 0005\r\n"), std::string::npos);
        EXPECT_EQ(first.find("Transfer-Encoding:"), std::string::npos);
        EXPECT_EQ(body(first), "hel");
        EXPECT_EQ(deliver(session_->receiveOutput("lo")), "lo");
        EXPECT_TRUE(ended<HttpSession::ReadOutput>());
        receiveOutput("");
        EXPECT_TRUE(std::holds_alternative<HttpSession::ReadOutput>(session_->emit()));
        EXPECT_TRUE(std::holds_alternative<HttpSession::Read>(session_->endOutput()));
    }

    TEST_F(HttpSessionTest, FramesBinaryChunksWithHexadecimalLengths) {
        startCgi(Requests::rawGet("/cgi/app.py"));
        const std::string response = deliver(session_->receiveOutput("Content-Type: application/octet-stream\n\n"));
        EXPECT_TRUE(body(response).empty());
        receiveOutput("");

        const std::string content = pattern(32);
        EXPECT_EQ(deliver(session_->receiveOutput(content)), "20\r\n" + content + "\r\n");
        EXPECT_TRUE(ended<HttpSession::ReadOutput>());
        EXPECT_EQ(deliver(session_->endOutput()), "0\r\n\r\n");
    }

    TEST_F(HttpSessionTest, AnswersBadGatewayForEmptyOrIncompleteCgiHeaders) {
        for (const std::string output : {"", "Content-Type: text/plain\r\n"}) {
            SCOPED_TRACE(output);
            startCgi(Requests::rawGet("/cgi/app.py"));
            receiveOutput(output);

            const std::string response = deliver(session_->endOutput());

            EXPECT_TRUE(response.starts_with("HTTP/1.1 502 "));
            EXPECT_TRUE(ended<HttpSession::Read>());
        }
    }

    TEST_F(HttpSessionTest, AnswersBadGatewayAsSoonAsHeadersAreInvalid) {
        for (const std::string output :
             {"nonsense\r\n\r\n", "\r\n", "Content-Length: nope\r\n\r\n", "Status: 100 Continue\r\n\r\n"}) {
            SCOPED_TRACE(output);
            startCgi(Requests::rawGet("/cgi/app.py"));

            const std::string response = deliver(session_->receiveOutput(output));

            EXPECT_TRUE(response.starts_with("HTTP/1.1 502 "));
            EXPECT_TRUE(ended<HttpSession::Read>());
        }
    }

    TEST_F(HttpSessionTest, RejectsOversizedHeadersBeforeSendingAResponse) {
        startCgi(Requests::rawGet("/cgi/app.py"));
        const std::string response = deliver(session_->receiveOutput("X-Large: " + std::string(kChunkSize, 'x')));

        EXPECT_TRUE(response.starts_with("HTTP/1.1 502 "));
    }

    TEST_F(HttpSessionTest, WritesBadGatewayBeforeAnsweringAPipelinedRequest) {
        const std::string content = pattern(3 * kChunkSize);
        Files::write(root_ / "file.bin", content);
        startCgi(Requests::rawGet("/cgi/app.py") + Requests::rawGet("/file.bin"));

        HttpSession::Action action = session_->receiveOutput("nonsense\r\n\r\n");
        const auto* write = std::get_if<HttpSession::Write>(&action);

        ASSERT_NE(write, nullptr);
        EXPECT_TRUE(write->bytes.starts_with("HTTP/1.1 502 "));

        action = session_->emit();
        ASSERT_TRUE(std::holds_alternative<HttpSession::Write>(action));
        const std::string response = deliver(std::move(action));

        EXPECT_TRUE(response.starts_with("HTTP/1.1 200 "));
        EXPECT_EQ(body(response), content);
        EXPECT_TRUE(ended<HttpSession::Read>());
    }

    TEST_F(HttpSessionTest, DoesNotReuseTheOutputOfThePreviousCgi) {
        startCgi(Requests::rawGet("/cgi/app.py"));
        deliver(session_->receiveOutput("Content-Type: text/plain\r\n\r\nfirst"));
        deliver(session_->endOutput());
        ASSERT_TRUE(ended<HttpSession::Read>());

        startCgi(Requests::rawGet("/cgi/app.py"));
        const std::string response = deliver(session_->endOutput());

        EXPECT_TRUE(response.starts_with("HTTP/1.1 502 "));
    }

    TEST_F(HttpSessionTest, DropsPartialCgiHeadersAfterAFailure) {
        startCgi(Requests::rawGet("/cgi/app.py"));
        receiveOutput("Content-Type: text/plain\r\n");
        EXPECT_TRUE(deliver(session_->fail(HttpSession::CgiFailure::Timeout)).starts_with("HTTP/1.1 504 "));
        ASSERT_TRUE(ended<HttpSession::Read>());

        startCgi(Requests::rawGet("/cgi/app.py"));
        const std::string response = deliver(session_->endOutput());

        EXPECT_TRUE(response.starts_with("HTTP/1.1 502 "));
    }

    TEST_F(HttpSessionTest, StreamsMultipleMegabytesWithoutCollectingOutput) {
        constexpr std::size_t kMegabyte = 1024 * 1024;
        constexpr std::size_t kOutputMegabytes = 3;
        const std::string megabyte(kMegabyte, 'x');
        startCgi(Requests::rawGet("/cgi/app.py"));
        deliver(session_->receiveOutput("Content-Type: application/octet-stream\r\n\r\n"));

        for (std::size_t chunk = 0; chunk < kOutputMegabytes; ++chunk) {
            HttpSession::Action action = session_->receiveOutput(megabyte);
            const auto* stream = std::get_if<HttpSession::Stream>(&action);
            ASSERT_NE(stream, nullptr) << chunk;
            EXPECT_EQ(stream->bytes, "100000\r\n" + megabyte + "\r\n");
            ASSERT_TRUE(std::holds_alternative<HttpSession::ReadOutput>(session_->emit()));
        }

        EXPECT_EQ(deliver(session_->endOutput()), "0\r\n\r\n");
        EXPECT_TRUE(ended<HttpSession::Read>());
    }

    TEST_F(HttpSessionTest, MapsCgiFailuresToStatuses) {
        const std::pair<HttpSession::CgiFailure, std::string> cases[] = {
            {HttpSession::CgiFailure::Busy, "503"},
            {HttpSession::CgiFailure::Launch, "502"},
            {HttpSession::CgiFailure::Output, "502"},
            {HttpSession::CgiFailure::Timeout, "504"},
        };

        for (const auto& [failure, status] : cases) {
            SCOPED_TRACE(status);
            startCgi(Requests::rawGet("/cgi/app.py"));

            const std::string response = deliver(session_->fail(failure));

            EXPECT_TRUE(response.starts_with("HTTP/1.1 " + status + " "));
            EXPECT_TRUE(ended<HttpSession::Read>());
        }
    }

    TEST_F(HttpSessionTest, ClosesOnAnyCgiFailureAfterHeadersWereSent) {
        for (const auto failure :
             {HttpSession::CgiFailure::Busy,
              HttpSession::CgiFailure::Launch,
              HttpSession::CgiFailure::Output,
              HttpSession::CgiFailure::Timeout}) {
            session_ = std::make_unique<HttpSession>(server_, ConnectionInfo{"10.0.0.7", 8080}, readFakeNow);
            startCgi(Requests::rawGet("/cgi/app.py"));
            deliver(session_->receiveOutput("Content-Type: text/plain\r\n\r\n"));

            EXPECT_TRUE(std::holds_alternative<HttpSession::Close>(session_->fail(failure)));
        }
    }

    TEST_F(HttpSessionTest, ClosesWhenContentLengthIsNotReachedAtEof) {
        startCgi(Requests::rawGet("/cgi/app.py"));
        deliver(session_->receiveOutput("Content-Type: text/plain\r\nContent-Length: 5\r\n\r\nhel"));

        EXPECT_TRUE(std::holds_alternative<HttpSession::Close>(session_->endOutput()));
    }

    TEST_F(HttpSessionTest, WritesRemainingContentLengthBeforeClosingOnExcessBody) {
        startCgi(Requests::rawGet("/cgi/app.py") + Requests::rawGet("/"));
        const std::string first =
            deliver(session_->receiveOutput("Content-Type: text/plain\r\nContent-Length: 5\r\n\r\nhe"));

        EXPECT_EQ(body(first), "he");
        HttpSession::Action action = session_->receiveOutput("llo and excess");
        ASSERT_TRUE(std::holds_alternative<HttpSession::Write>(action));
        EXPECT_EQ(deliver(std::move(action)), "llo");
        EXPECT_TRUE(ended<HttpSession::Close>());
    }

    TEST_F(HttpSessionTest, WritesOnlyContentLengthWhenFirstOutputHasExcessBody) {
        for (const std::string length : {"0", "3"}) {
            session_ = std::make_unique<HttpSession>(server_, ConnectionInfo{"10.0.0.7", 8080}, readFakeNow);
            startCgi(Requests::rawGet("/cgi/app.py") + Requests::rawGet("/"));
            HttpSession::Action action =
                session_->receiveOutput("Content-Type: text/plain\r\nContent-Length: " + length + "\r\n\r\nfour");

            ASSERT_TRUE(std::holds_alternative<HttpSession::Write>(action));
            const std::string response = deliver(std::move(action));

            EXPECT_TRUE(response.starts_with("HTTP/1.1 200 "));
            EXPECT_EQ(body(response), length == "0" ? "" : "fou");
            EXPECT_TRUE(ended<HttpSession::Close>());
        }
    }

    TEST_F(HttpSessionTest, ClosesImmediatelyWhenContentLengthWasAlreadySent) {
        startCgi(Requests::rawGet("/cgi/app.py"));
        deliver(session_->receiveOutput("Content-Type: text/plain\r\nContent-Length: 3\r\n\r\nfou"));

        EXPECT_TRUE(std::holds_alternative<HttpSession::Close>(session_->receiveOutput("r")));
    }

    TEST_F(HttpSessionTest, WaitsForEofEvenWithZeroContentLength) {
        startCgi(Requests::rawGet("/cgi/app.py"));
        const std::string response = deliver(session_->receiveOutput("Content-Length: 0\r\n\r\n"));

        EXPECT_TRUE(body(response).empty());
        EXPECT_TRUE(ended<HttpSession::ReadOutput>());
        EXPECT_TRUE(std::holds_alternative<HttpSession::Read>(session_->endOutput()));
    }

    TEST_F(HttpSessionTest, HeadDrainsCgiWithoutSendingBodyOrInventingLength) {
        startCgi("HEAD /cgi/app.py HTTP/1.1\r\nHost: localhost\r\n\r\n");
        const std::string response = deliver(session_->receiveOutput("Content-Type: text/plain\r\n\r\nhello"));

        EXPECT_EQ(response.find("Content-Length:"), std::string::npos);
        EXPECT_EQ(response.find("Transfer-Encoding:"), std::string::npos);
        EXPECT_TRUE(body(response).empty());
        EXPECT_TRUE(ended<HttpSession::ReadOutput>());
        receiveOutput("discard this too");
        EXPECT_TRUE(std::holds_alternative<HttpSession::ReadOutput>(session_->emit()));
        EXPECT_TRUE(std::holds_alternative<HttpSession::Read>(session_->endOutput()));
    }

    TEST_F(HttpSessionTest, HeadPreservesCgiLengthAndDoesNotValidateDiscardedBody) {
        startCgi("HEAD /cgi/app.py HTTP/1.1\r\nHost: localhost\r\n\r\n");
        const std::string response =
            deliver(session_->receiveOutput("Content-Type: text/plain\r\nContent-Length: 5\r\n\r\nignored body"));

        EXPECT_NE(response.find("Content-Length: 5\r\n"), std::string::npos);
        EXPECT_TRUE(body(response).empty());
        receiveOutput("more ignored bytes");
        EXPECT_TRUE(std::holds_alternative<HttpSession::Read>(session_->endOutput()));
    }

    TEST_F(HttpSessionTest, DrainsCgiForStatusesThatForbidBody) {
        for (const std::string status : {"204 No Content", "304 Not Modified"}) {
            startCgi(Requests::rawGet("/cgi/app.py"));
            const std::string response = deliver(session_->receiveOutput(
                "Status: " + status + "\r\nContent-Type: text/plain\r\nContent-Length: 1\r\n\r\nhidden body"
            ));

            EXPECT_TRUE(response.starts_with("HTTP/1.1 " + status));
            EXPECT_EQ(response.find("Transfer-Encoding:"), std::string::npos);
            EXPECT_TRUE(body(response).empty());
            EXPECT_TRUE(ended<HttpSession::ReadOutput>());
            receiveOutput("more hidden body");
            EXPECT_TRUE(std::holds_alternative<HttpSession::Read>(session_->endOutput()));
        }
    }

    TEST_F(HttpSessionTest, ClosesAfterCgiWhenTheClientAsksForIt) {
        startCgi(Requests::rawGet("/cgi/app.py", "close"));
        const std::string response = deliver(session_->receiveOutput("Content-Type: text/plain\r\n\r\nhello"));

        EXPECT_TRUE(closes(response));
        EXPECT_TRUE(ended<HttpSession::ReadOutput>());
        EXPECT_EQ(deliver(session_->endOutput()), "0\r\n\r\n");
        EXPECT_TRUE(ended<HttpSession::Close>());
    }

    TEST_F(HttpSessionTest, AnswersARequestPipelinedBehindCgi) {
        startCgi(Requests::rawGet("/cgi/app.py") + Requests::rawGet("/"));
        const std::string first = deliver(session_->receiveOutput("Content-Type: text/plain\r\n\r\ncgi"));

        EXPECT_EQ(body(first), "3\r\ncgi\r\n");
        EXPECT_TRUE(ended<HttpSession::ReadOutput>());
        const std::string rest = deliver(session_->endOutput());

        EXPECT_TRUE(rest.starts_with("0\r\n\r\nHTTP/1.1 200 "));
        EXPECT_TRUE(rest.ends_with("\r\n\r\nhello"));
        EXPECT_TRUE(ended<HttpSession::Read>());
    }
} // namespace
