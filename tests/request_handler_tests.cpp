#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <variant>

#include <gtest/gtest.h>

#include "config/ServerConfig.hpp"
#include "http/RequestDispatcher.hpp"

namespace {

    namespace fs = std::filesystem;

    class RequestHandlerTest : public ::testing::Test {
    protected:
        fs::path root_;
        ServerConfig server_{};
        const std::string fileBody_ = "<h1>Hello</h1>\n";

        void SetUp() override {
            std::string pattern = (fs::temp_directory_path() / "webserv-handler-XXXXXX").string();

            char* directory = ::mkdtemp(pattern.data());
            ASSERT_NE(directory, nullptr);

            root_ = directory;

            server_.root = root_;
            server_.index = "index.html";
            server_.clientMaxBodySize = 8;

            LocationConfig location{};
            location.path = "/";
            location.allowedMethods = {HttpMethod::Get};
            server_.locations = {location};

            std::ofstream file;
            file.exceptions(std::ios::failbit | std::ios::badbit);
            file.open(root_ / "index.html", std::ios::binary);
            file << fileBody_;
            file.close();
        }

        void TearDown() override {
            if (root_.empty()) {
                return;
            }

            std::error_code error;
            fs::remove_all(root_, error);
            EXPECT_FALSE(error) << error.message();
        }

        HttpRequest
        requestFor(const std::string& path, HttpMethod method = HttpMethod::Get, const std::string& body = "") const {
            HttpRequest request{};
            request.method = method;
            request.target = path;
            request.path = path;
            request.version = "HTTP/1.1";
            request.body = body;
            request.headers.set("Host", "localhost");
            return request;
        }
    };

    TEST_F(RequestHandlerTest, Returns200ForExistingFile) {
        const auto result = RequestDispatcher::dispatch(requestFor("/index.html"), server_);

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::OK);
        EXPECT_EQ(response.body, fileBody_);
    }

    TEST_F(RequestHandlerTest, Returns404ForMissingFile) {
        const auto result = RequestDispatcher::dispatch(requestFor("/missing.html"), server_);

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::NotFound);
        EXPECT_FALSE(response.body.empty());
    }

    TEST_F(RequestHandlerTest, Returns404WhenNoLocationMatches) {
        server_.locations.front().path = "/private";

        const auto result = RequestDispatcher::dispatch(requestFor("/index.html"), server_);

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::NotFound);
    }

    TEST_F(RequestHandlerTest, Returns405WithAllowHeader) {
        const auto result = RequestDispatcher::dispatch(requestFor("/index.html", HttpMethod::Post), server_);

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::MethodNotAllowed);

        const auto allow = response.headers.get("Allow");
        ASSERT_TRUE(allow.has_value());
        EXPECT_EQ(*allow, "GET");
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
            requestFor("/index.html", HttpMethod::Get, std::string(test.bodySize, 'a')), server_
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

        const auto result = RequestDispatcher::dispatch(requestFor("/index.html", HttpMethod::Get, "12345"), server_);

        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::PayloadTooLarge);
    }

    TEST_F(RequestHandlerTest, LocationCanIncreaseBodyLimit) {
        server_.clientMaxBodySize = 4;
        server_.locations.front().clientMaxBodySize = 8;

        const auto result = RequestDispatcher::dispatch(requestFor("/index.html", HttpMethod::Get, "12345"), server_);

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

        // The file does not exist: the redirect must happen before file access.
        const auto result = RequestDispatcher::dispatch(requestFor("/old"), server_);

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
