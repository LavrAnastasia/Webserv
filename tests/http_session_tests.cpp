#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <variant>

#include <gtest/gtest.h>

#include "config/ServerConfig.hpp"
#include "http/HttpSession.hpp"
#include "includes/Files.hpp"
#include "includes/Requests.hpp"
#include "includes/Responses.hpp"
#include "includes/TempDirectory.hpp"

namespace {
    namespace fs = std::filesystem;

    class HttpSessionTest : public testing::Test {
    protected:
        TempDirectory temp_{"webserv-session"};
        ServerConfig server_{};
        std::unique_ptr<HttpSession> session_;

        void SetUp() override {
            const fs::path root = temp_.path() / "root";
            Files::write(root / "index.html", "hello");
            Files::write(root / "cgi/app.py", "print()");

            LocationConfig site{};
            site.path = "/";
            site.allowedMethods = {HttpMethod::Get, HttpMethod::Head};

            LocationConfig cgi{};
            cgi.path = "/cgi";
            cgi.allowedMethods = {HttpMethod::Get};
            cgi.cgi.emplace(".py", CgiConfig{".py", fs::path("/usr/bin/python3")});

            server_.root = root;
            server_.locations = {site, cgi};
            session_ = std::make_unique<HttpSession>(server_, ConnectionInfo{"10.0.0.7", 8080});
        }

        static std::string respond(HttpSession::Action action) {
            auto* output = std::get_if<HttpSerializer::Output>(&action);

            if (output == nullptr) {
                ADD_FAILURE() << "Expected a response";
                return {};
            }

            std::string text = output->headers;

            if (output->body) {
                text.append(Responses::read(*output->body));
            }

            return text;
        }

        static bool closes(const std::string& response) {
            return response.find("Connection: close\r\n") != std::string::npos;
        }
    };

    TEST_F(HttpSessionTest, WaitsForTheWholeRequestBeforeResponding) {
        EXPECT_TRUE(std::holds_alternative<HttpSession::Read>(session_->receive("GET / HTTP/1.1\r\nHo")));

        const std::string response = respond(session_->receive("st: localhost\r\n\r\n"));

        EXPECT_TRUE(response.starts_with("HTTP/1.1 200 "));
    }

    TEST_F(HttpSessionTest, ClosesAfterAnsweringAMalformedRequest) {
        const std::string response = respond(session_->receive("GARBAGE\r\n\r\n"));

        EXPECT_TRUE(response.starts_with("HTTP/1.1 400 "));
        EXPECT_TRUE(closes(response));
        EXPECT_TRUE(std::holds_alternative<HttpSession::Close>(session_->finish()));
    }

    TEST_F(HttpSessionTest, KeepsTheConnectionAndAnswersTheNextRequest) {
        const std::string first = respond(session_->receive(Requests::rawGet("/")));

        EXPECT_TRUE(first.starts_with("HTTP/1.1 200 "));
        EXPECT_FALSE(closes(first));
        ASSERT_TRUE(std::holds_alternative<HttpSession::Read>(session_->finish()));

        const std::string second = respond(session_->receive(Requests::rawGet("/missing")));

        EXPECT_TRUE(second.starts_with("HTTP/1.1 404 "));
    }

    TEST_F(HttpSessionTest, ClosesWhenTheClientAsksForIt) {
        const std::string response = respond(session_->receive(Requests::rawGet("/", "close")));

        EXPECT_TRUE(closes(response));
        EXPECT_TRUE(std::holds_alternative<HttpSession::Close>(session_->finish()));
    }

    TEST_F(HttpSessionTest, HeadOmitsTheBodyOnlyForThatRequest) {
        HttpSession::Action head = session_->receive("HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n");
        auto* output = std::get_if<HttpSerializer::Output>(&head);

        ASSERT_NE(output, nullptr);
        EXPECT_FALSE(output->body.has_value());
        EXPECT_NE(output->headers.find("Content-Length: 5\r\n"), std::string::npos);
        ASSERT_TRUE(std::holds_alternative<HttpSession::Read>(session_->finish()));

        const std::string get = respond(session_->receive(Requests::rawGet("/")));

        EXPECT_TRUE(get.ends_with("\r\n\r\nhello"));
    }

    TEST_F(HttpSessionTest, SendsTheErrorBodyForAMalformedRequestAfterHead) {
        respond(session_->receive("HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n"));
        ASSERT_TRUE(std::holds_alternative<HttpSession::Read>(session_->finish()));

        HttpSession::Action action = session_->receive("GARBAGE\r\n\r\n");
        auto* output = std::get_if<HttpSerializer::Output>(&action);

        ASSERT_NE(output, nullptr);
        EXPECT_TRUE(output->body.has_value());
    }

    TEST_F(HttpSessionTest, AnswersTimeoutWhileWaitingForARequestAndCloses) {
        EXPECT_TRUE(std::holds_alternative<HttpSession::Read>(session_->receive("GET / HT")));

        const std::string response = respond(session_->expire());

        EXPECT_TRUE(response.starts_with("HTTP/1.1 408 "));
        EXPECT_TRUE(closes(response));
        EXPECT_TRUE(std::holds_alternative<HttpSession::Close>(session_->finish()));
    }

    TEST_F(HttpSessionTest, ClosesWithoutAnswerOnTimeoutWhileResponding) {
        respond(session_->receive(Requests::rawGet("/")));

        EXPECT_TRUE(std::holds_alternative<HttpSession::Close>(session_->expire()));
    }

    TEST_F(HttpSessionTest, DoesNotAnswerBytesReceivedWhileResponding) {
        respond(session_->receive(Requests::rawGet("/")));

        EXPECT_TRUE(std::holds_alternative<HttpSession::Read>(session_->receive(Requests::rawGet("/"))));
    }

    TEST_F(HttpSessionTest, PassesTheClientAddressToCgi) {
        HttpSession::Action action = session_->receive(Requests::rawGet("/cgi/app.py"));
        auto* request = std::get_if<CgiRequest>(&action);

        ASSERT_NE(request, nullptr);
        EXPECT_NE(std::ranges::find(request->env, "REMOTE_ADDR=10.0.0.7"), request->env.end());
    }
} // namespace
