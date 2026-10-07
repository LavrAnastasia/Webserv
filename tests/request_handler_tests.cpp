#include <cstddef>
#include <filesystem>
#include <string>
#include <variant>

#include <gtest/gtest.h>

#include "config/ServerConfig.hpp"
#include "http/HttpParser.hpp"
#include "http/RequestDispatcher.hpp"
#include "includes/Files.hpp"
#include "includes/Requests.hpp"
#include "includes/TempDirectory.hpp"

namespace {

    namespace fs = std::filesystem;

    class RequestHandlerTest : public ::testing::Test {
    protected:
        TempDirectory temp_{"webserv-handler"};
        fs::path root_;
        ServerConfig server_{};
        const std::string fileBody_ = "<h1>Hello</h1>\n";

        void SetUp() override {
            root_ = temp_.path();

            server_.root = root_;
            server_.index = "index.html";
            server_.clientMaxBodySize = 8;

            LocationConfig location{};
            location.path = "/";
            location.allowedMethods = {HttpMethod::Get};
            server_.locations = {location};

            Files::write(root_ / "index.html", fileBody_);
        }
    };

    TEST_F(RequestHandlerTest, LocationLimitStillAppliesWhenParserAcceptsMore) {
        LocationConfig upload{};
        upload.path = "/upload";
        upload.allowedMethods = {HttpMethod::Post};
        upload.clientMaxBodySize = 64;
        server_.locations.push_back(upload);

        HttpParser parser(server_.maxBodySize());
        const std::string request =
            "GET /index.html HTTP/1.1\r\nHost: localhost\r\nContent-Length: 16\r\n\r\n" + std::string(16, 'a');
        const ParseResult parsed = parser.append(request.data(), request.size());

        ASSERT_TRUE(std::holds_alternative<Complete>(parsed));
        const auto result = RequestDispatcher::dispatch(std::get<Complete>(parsed).request, server_, ConnectionInfo{});

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        EXPECT_EQ(std::get<HttpResponse>(result).status, HttpStatus::PayloadTooLarge);
    }

    TEST_F(RequestHandlerTest, Returns200ForExistingFile) {
        const auto result = RequestDispatcher::dispatch(Requests::get("/index.html"), server_, ConnectionInfo{});

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::OK);
        EXPECT_EQ(response.body, fileBody_);
    }

    TEST_F(RequestHandlerTest, Returns404ForMissingFile) {
        const auto result = RequestDispatcher::dispatch(Requests::get("/missing.html"), server_, ConnectionInfo{});

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::NotFound);
        EXPECT_FALSE(response.body.empty());
    }

    TEST_F(RequestHandlerTest, Returns404WhenNoLocationMatches) {
        server_.locations.front().path = "/private";

        const auto result = RequestDispatcher::dispatch(Requests::get("/index.html"), server_, ConnectionInfo{});

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::NotFound);
    }

    TEST_F(RequestHandlerTest, Returns405WithAllowHeader) {
        const auto result = RequestDispatcher::dispatch(Requests::post("/index.html"), server_, ConnectionInfo{});

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::MethodNotAllowed);

        const auto allow = response.headers.get("Allow");
        ASSERT_TRUE(allow.has_value());
        EXPECT_EQ(*allow, "GET, HEAD");
    }

    struct BodyLimitCase {
        const char* name;
        std::size_t bodySize;
        HttpStatus expectedStatus;
    };

    class RequestHandlerBodyLimitTest : public RequestHandlerTest,
                                        public ::testing::WithParamInterface<BodyLimitCase> {};

    TEST_P(RequestHandlerBodyLimitTest, ChecksBodySize) {
        const auto& test = GetParam();

        const auto result = RequestDispatcher::dispatch(
            Requests::get("/index.html", std::string(test.bodySize, 'a')), server_, ConnectionInfo{}
        );

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, test.expectedStatus);

        if (test.expectedStatus == HttpStatus::OK) {
            EXPECT_EQ(response.body, fileBody_);
        }
    }

    INSTANTIATE_TEST_SUITE_P(
        BodyLimits,
        RequestHandlerBodyLimitTest,
        ::testing::Values(
            BodyLimitCase{"Empty", 0, HttpStatus::OK},
            BodyLimitCase{"BelowLimit", 7, HttpStatus::OK},
            BodyLimitCase{"ExactlyAtLimit", 8, HttpStatus::OK},
            BodyLimitCase{"AboveLimit", 9, HttpStatus::PayloadTooLarge}
        ),
        [](const ::testing::TestParamInfo<BodyLimitCase>& info) { return std::string(info.param.name); }
    );

    TEST_F(RequestHandlerTest, UsesSmallerLocationBodyLimit) {
        server_.locations.front().clientMaxBodySize = 4;

        const auto result =
            RequestDispatcher::dispatch(Requests::get("/index.html", "12345"), server_, ConnectionInfo{});

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::PayloadTooLarge);
    }

    TEST_F(RequestHandlerTest, LocationCanIncreaseBodyLimit) {
        server_.clientMaxBodySize = 4;
        server_.locations.front().clientMaxBodySize = 8;

        const auto result =
            RequestDispatcher::dispatch(Requests::get("/index.html", "12345"), server_, ConnectionInfo{});

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::OK);
        EXPECT_EQ(response.body, fileBody_);
    }

    struct RedirectCase {
        const char* name;
        HttpStatus status;
        const char* target;
    };

    class RequestHandlerRedirectTest : public RequestHandlerTest, public ::testing::WithParamInterface<RedirectCase> {};

    TEST_P(RequestHandlerRedirectTest, ReturnsStatusAndLocation) {
        const auto& test = GetParam();

        server_.locations.front().redirect = RedirectConfig{test.status, std::string(test.target)};

        const auto result = RequestDispatcher::dispatch(Requests::get("/old"), server_, ConnectionInfo{});

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, test.status);

        const auto location = response.headers.get("Location");
        ASSERT_TRUE(location.has_value());
        EXPECT_EQ(*location, test.target);
    }

    INSTANTIATE_TEST_SUITE_P(
        Redirects,
        RequestHandlerRedirectTest,
        ::testing::Values(
            RedirectCase{"MovedPermanently301", HttpStatus::MovedPermanently, "/new"},
            RedirectCase{"Found302", HttpStatus::Found, "https://example.com/new"},
            RedirectCase{"SeeOther303", HttpStatus::SeeOther, "/result"},
            RedirectCase{"TemporaryRedirect307", HttpStatus::TemporaryRedirect, "/temporary?x=1"},
            RedirectCase{"PermanentRedirect308", HttpStatus::PermanentRedirect, "https://example.com/permanent"}
        ),
        [](const ::testing::TestParamInfo<RedirectCase>& info) { return std::string(info.param.name); }
    );

} // namespace
