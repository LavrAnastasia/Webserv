#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <variant>

#include <gtest/gtest.h>

#include "config/ServerConfig.hpp"
#include "http/HttpSession.hpp"
#include "includes/Files.hpp"
#include "includes/Requests.hpp"
#include "includes/TempDirectory.hpp"

namespace {
    namespace fs = std::filesystem;

    constexpr std::size_t kChunkSize = 64 * 1024;

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
            cgi.allowedMethods = {HttpMethod::Get};
            cgi.cgi.emplace(".py", CgiConfig{".py", fs::path("/usr/bin/python3")});

            server_.root = root_;
            server_.locations = {site, cgi};
            session_ = std::make_unique<HttpSession>(server_, ConnectionInfo{"10.0.0.7", 8080});
        }

        std::string deliver(HttpSession::Action action) {
            std::string text;
            int writes = 0;

            while (auto* write = std::get_if<HttpSession::Write>(&action)) {
                if (write->bytes.empty() || ++writes > 1024) {
                    ADD_FAILURE() << "Response never ends or emits empty writes";
                    break;
                }

                text += write->bytes;
                action = session_->emit();
            }

            end_ = std::move(action);
            return text;
        }

        template <typename Action> bool ended() const { return std::holds_alternative<Action>(end_); }

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

    TEST_F(HttpSessionTest, AnswersTimeoutWhileWaitingForARequestAndCloses) {
        EXPECT_TRUE(std::holds_alternative<HttpSession::Read>(session_->receive("GET / HT")));

        const std::string response = deliver(session_->expire());

        EXPECT_TRUE(response.starts_with("HTTP/1.1 408 "));
        EXPECT_TRUE(closes(response));
        EXPECT_TRUE(ended<HttpSession::Close>());
    }

    TEST_F(HttpSessionTest, ClosesWithoutAnswerOnTimeoutWhileResponding) {
        ASSERT_TRUE(std::holds_alternative<HttpSession::Write>(session_->receive(Requests::rawGet("/"))));

        EXPECT_TRUE(std::holds_alternative<HttpSession::Close>(session_->expire()));
    }

    TEST_F(HttpSessionTest, DoesNotAnswerBytesReceivedWhileResponding) {
        ASSERT_TRUE(std::holds_alternative<HttpSession::Write>(session_->receive(Requests::rawGet("/"))));

        EXPECT_TRUE(std::holds_alternative<HttpSession::Read>(session_->receive(Requests::rawGet("/"))));
    }

    TEST_F(HttpSessionTest, PassesTheClientAddressToCgi) {
        HttpSession::Action action = session_->receive(Requests::rawGet("/cgi/app.py"));
        auto* request = std::get_if<CgiRequest>(&action);

        ASSERT_NE(request, nullptr);
        EXPECT_NE(std::ranges::find(request->env, "REMOTE_ADDR=10.0.0.7"), request->env.end());
    }
} // namespace
