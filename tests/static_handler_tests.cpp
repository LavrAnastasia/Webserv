#include <filesystem>
#include <string>
#include <variant>

#include <gtest/gtest.h>

#include "config/ServerConfig.hpp"
#include "http/RequestDispatcher.hpp"
#include "includes/Files.hpp"
#include "includes/Requests.hpp"
#include "includes/TempDirectory.hpp"

namespace {

    namespace fs = std::filesystem;

    class StaticHandlerTest : public ::testing::Test {
    protected:
        TempDirectory temp_{"webserv-static"};
        fs::path root_;
        fs::path outside_;
        ServerConfig server_{};

        const std::string outsideBody_ = "outside secret\n";

        void SetUp() override {
            root_ = temp_.path() / "root";

            outside_ = temp_.path() / "root-other";

            fs::create_directories(root_);
            fs::create_directories(outside_);
            Files::write(outside_ / "secret.txt", outsideBody_);
            server_.root = root_;
            server_.index = "index.html";

            LocationConfig location{};
            location.path = "/";
            location.allowedMethods = {HttpMethod::Get, HttpMethod::Delete};
            location.autoindex = false;

            server_.locations = {location};
        }

        void expectHeader(const HttpResponse& response, const std::string& name, const std::string& expected) const {
            const auto value = response.headers.get(name);
            ASSERT_TRUE(value.has_value()) << name;
            EXPECT_EQ(*value, expected) << name;
        }
    };


    struct FileContentsCase {
        const char* name;
        std::string body;
    };

    class StaticHandlerFileContentsTest : public StaticHandlerTest,
                                          public ::testing::WithParamInterface<FileContentsCase> {};

    TEST_P(StaticHandlerFileContentsTest, ReturnsExactFileContents) {
        const auto& test = GetParam();
        Files::write(root_ / "payload.bin", test.body);

        const auto result = RequestDispatcher::dispatch(Requests::get("/payload.bin"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        ASSERT_EQ(response.status, HttpStatus::OK);
        EXPECT_EQ(response.body, test.body);
        expectHeader(response, "Content-Type", "application/octet-stream");
    }

    INSTANTIATE_TEST_SUITE_P(
        FileContents,
        StaticHandlerFileContentsTest,
        ::testing::Values(
            FileContentsCase{"Empty", ""},
            FileContentsCase{"Text", "Hello\nSecond line\r\n"},
            FileContentsCase{"Binary", std::string("\x00\x01\x7f\x80\xff", 5)},
            FileContentsCase{"Exactly64KiB", std::string(64 * 1024, 'a')},
            FileContentsCase{"MultipleReadChunks", std::string(64 * 1024, 'a') + std::string(64 * 1024, 'b') + "tail"}
        ),
        [](const ::testing::TestParamInfo<FileContentsCase>& info) { return std::string(info.param.name); }
    );

    TEST_F(StaticHandlerTest, Returns404ForMissingFile) {
        const auto result = RequestDispatcher::dispatch(Requests::get("/missing.txt"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::NotFound);
        EXPECT_FALSE(response.body.empty());
    }

    TEST_F(StaticHandlerTest, Returns404ForUnroutableRequestPath) {
        for (const std::string path : {"", "relative.txt"}) {
            SCOPED_TRACE(path);

            const auto result = RequestDispatcher::dispatch(Requests::get(path), server_);
            ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
            const auto& response = std::get<HttpResponse>(result);

            EXPECT_EQ(response.status, HttpStatus::NotFound);
        }
    }


    TEST_F(StaticHandlerTest, ReturnsIndexForRootDirectory) {
        const std::string body = "<h1>Home</h1>\n";
        Files::write(root_ / "index.html", body);

        const auto result = RequestDispatcher::dispatch(Requests::get("/"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        ASSERT_EQ(response.status, HttpStatus::OK);
        EXPECT_EQ(response.body, body);
        expectHeader(response, "Content-Type", "text/html; charset=utf-8");
    }

    TEST_F(StaticHandlerTest, UsesConfiguredNestedIndexBeforeAutoindex) {
        server_.locations.front().index = "home.html";
        server_.locations.front().autoindex = true;

        const std::string body = "<h1>Documentation</h1>\n";
        Files::write(root_ / "docs" / "home.html", body);
        Files::write(root_ / "docs" / "index.html", "wrong index");

        const auto result = RequestDispatcher::dispatch(Requests::get("/docs/"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        ASSERT_EQ(response.status, HttpStatus::OK);
        EXPECT_EQ(response.body, body);
    }

    TEST_F(StaticHandlerTest, RedirectsDirectoryAndPreservesQuery) {
        fs::create_directories(root_ / "my docs");

        auto request = Requests::get("/my docs");
        request.query = "sort=name&order=asc";
        request.target = "/my%20docs?" + request.query;

        const auto result = RequestDispatcher::dispatch(request, server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::MovedPermanently);
        expectHeader(response, "Location", "/my%20docs/?sort=name&order=asc");
    }

    TEST_F(StaticHandlerTest, ForbidsDirectoryWithoutIndexWhenAutoindexIsOff) {
        fs::create_directories(root_ / "docs");

        for (const std::string index : {"index.html", ""}) {
            SCOPED_TRACE(index);
            server_.locations.front().index = index;

            const auto result = RequestDispatcher::dispatch(Requests::get("/docs/"), server_);
            ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
            const auto& response = std::get<HttpResponse>(result);

            EXPECT_EQ(response.status, HttpStatus::Forbidden);
        }
    }

    TEST_F(StaticHandlerTest, ForbidsIndexThatIsDirectoryEvenWithAutoindex) {
        server_.locations.front().autoindex = true;
        fs::create_directories(root_ / "index.html");

        const auto result = RequestDispatcher::dispatch(Requests::get("/"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::Forbidden);
    }


    TEST_F(StaticHandlerTest, AutoindexSortsDirectoriesBeforeFiles) {
        server_.locations.front().autoindex = true;

        Files::write(root_ / "z.txt", "z");
        fs::create_directories(root_ / "z-dir");
        Files::write(root_ / "a.txt", "a");
        fs::create_directories(root_ / "a-dir");

        const auto result = RequestDispatcher::dispatch(Requests::get("/"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        ASSERT_EQ(response.status, HttpStatus::OK);
        expectHeader(response, "Content-Type", "text/html; charset=utf-8");

        const auto aDir = response.body.find("<a href=\"a-dir/\">a-dir/</a>");
        const auto zDir = response.body.find("<a href=\"z-dir/\">z-dir/</a>");
        const auto aFile = response.body.find("<a href=\"a.txt\">a.txt</a>");
        const auto zFile = response.body.find("<a href=\"z.txt\">z.txt</a>");

        ASSERT_NE(aDir, std::string::npos);
        ASSERT_NE(zDir, std::string::npos);
        ASSERT_NE(aFile, std::string::npos);
        ASSERT_NE(zFile, std::string::npos);

        EXPECT_LT(aDir, zDir);
        EXPECT_LT(zDir, aFile);
        EXPECT_LT(aFile, zFile);

        EXPECT_NE(response.body.find("<h1>Index of /</h1>"), std::string::npos);
        EXPECT_EQ(response.body.find("href=\"../\""), std::string::npos);
    }

    TEST_F(StaticHandlerTest, AutoindexWorksWithoutIndexAndAddsParentLink) {
        server_.locations.front().index = "";
        server_.locations.front().autoindex = true;
        Files::write(root_ / "docs" / "readme.txt", "documentation");

        const auto result = RequestDispatcher::dispatch(Requests::get("/docs/"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        ASSERT_EQ(response.status, HttpStatus::OK);
        EXPECT_NE(response.body.find("<a href=\"../\">../</a>"), std::string::npos);
        EXPECT_NE(response.body.find("<a href=\"readme.txt\">readme.txt</a>"), std::string::npos);
    }

    TEST_F(StaticHandlerTest, AutoindexEscapesHtmlAndEncodesLinks) {
        server_.locations.front().autoindex = true;
        Files::write(root_ / "<docs>" / "a & <b>.txt", "contents");

        const auto result = RequestDispatcher::dispatch(Requests::get("/<docs>/"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        ASSERT_EQ(response.status, HttpStatus::OK);
        EXPECT_NE(
            response.body.find(
                "<a href=\"a%20%26%20%3Cb%3E.txt\">"
                "a &amp; &lt;b&gt;.txt</a>"
            ),
            std::string::npos
        );
        EXPECT_NE(response.body.find("<h1>Index of /&lt;docs&gt;/</h1>"), std::string::npos);
        EXPECT_EQ(response.body.find("a & <b>.txt"), std::string::npos);
    }


    TEST_F(StaticHandlerTest, DeletesFileAndReturns204WithEmptyBody) {
        Files::write(root_ / "delete-me.txt", "temporary");

        const auto result = RequestDispatcher::dispatch(Requests::make(HttpMethod::Delete, "/delete-me.txt"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        ASSERT_EQ(response.status, HttpStatus::NoContent);
        EXPECT_TRUE(response.body.empty());
        EXPECT_FALSE(fs::exists(root_ / "delete-me.txt"));

        const auto repeatedResult =
            RequestDispatcher::dispatch(Requests::make(HttpMethod::Delete, "/delete-me.txt"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(repeatedResult));
        const auto& repeated = std::get<HttpResponse>(repeatedResult);

        EXPECT_EQ(repeated.status, HttpStatus::NotFound);
    }

    TEST_F(StaticHandlerTest, DeleteForbidsDirectoriesAndRoot) {
        Files::write(root_ / "docs" / "keep.txt", "keep");

        for (const std::string path : {"/", "/docs", "/docs/"}) {
            SCOPED_TRACE(path);

            const auto result = RequestDispatcher::dispatch(Requests::make(HttpMethod::Delete, path), server_);
            ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
            const auto& response = std::get<HttpResponse>(result);

            EXPECT_EQ(response.status, HttpStatus::Forbidden);
            EXPECT_TRUE(fs::is_directory(root_));
            EXPECT_TRUE(fs::is_directory(root_ / "docs"));
            EXPECT_EQ(Files::read(root_ / "docs" / "keep.txt"), "keep");
        }
    }

    TEST_F(StaticHandlerTest, DeleteRemovesSymlinkButPreservesOutsideTarget) {
        const fs::path link = root_ / "link.txt";
        fs::create_symlink(outside_ / "secret.txt", link);

        const auto result = RequestDispatcher::dispatch(Requests::make(HttpMethod::Delete, "/link.txt"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        ASSERT_EQ(response.status, HttpStatus::NoContent);
        EXPECT_TRUE(response.body.empty());

        EXPECT_FALSE(fs::exists(fs::symlink_status(link)));
        EXPECT_EQ(Files::read(outside_ / "secret.txt"), outsideBody_);
    }

    TEST_F(StaticHandlerTest, DeleteRemovesDanglingSymlink) {
        const fs::path link = root_ / "broken.txt";
        fs::create_symlink(outside_ / "missing.txt", link);
        ASSERT_TRUE(fs::is_symlink(fs::symlink_status(link)));

        const auto result = RequestDispatcher::dispatch(Requests::make(HttpMethod::Delete, "/broken.txt"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::NoContent);
        EXPECT_FALSE(fs::exists(fs::symlink_status(link)));
    }


    TEST_F(StaticHandlerTest, GetAllowsSymlinkWhoseTargetIsInsideRoot) {
        Files::write(root_ / "data" / "file.txt", "inside");
        fs::create_symlink(root_ / "data" / "file.txt", root_ / "link.txt");

        const auto result = RequestDispatcher::dispatch(Requests::get("/link.txt"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        ASSERT_EQ(response.status, HttpStatus::OK);
        EXPECT_EQ(response.body, "inside");
    }

    TEST_F(StaticHandlerTest, GetForbidsFileSymlinkPointingOutsideRoot) {
        fs::create_symlink(outside_ / "secret.txt", root_ / "link.txt");

        const auto result = RequestDispatcher::dispatch(Requests::get("/link.txt"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::Forbidden);
        EXPECT_EQ(response.body.find(outsideBody_), std::string::npos);
        EXPECT_EQ(Files::read(outside_ / "secret.txt"), outsideBody_);
    }

    TEST_F(StaticHandlerTest, IndexCannotEscapeRoot) {
        server_.locations.front().autoindex = true;
        fs::create_symlink(outside_ / "secret.txt", root_ / "index.html");

        const std::string indexes[] = {
            "index.html",
            "../root-other/secret.txt",
            (outside_ / "secret.txt").string(),
        };

        for (const auto& index : indexes) {
            SCOPED_TRACE(index);
            server_.locations.front().index = index;

            const auto result = RequestDispatcher::dispatch(Requests::get("/"), server_);
            ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
            const auto& response = std::get<HttpResponse>(result);

            EXPECT_EQ(response.status, HttpStatus::Forbidden);
            EXPECT_EQ(response.body.find(outsideBody_), std::string::npos);
        }

        EXPECT_EQ(Files::read(outside_ / "secret.txt"), outsideBody_);
    }

    class StaticHandlerRootGuardTest : public StaticHandlerTest, public ::testing::WithParamInterface<HttpMethod> {};

    TEST_P(StaticHandlerRootGuardTest, ForbidsDotDotEscapeToSimilarRootPrefix) {
        fs::create_directories(root_ / "docs");

        for (const std::string path : {"/../root-other/secret.txt", "/docs/../../root-other/secret.txt"}) {
            SCOPED_TRACE(path);

            const auto result = RequestDispatcher::dispatch(Requests::make(GetParam(), path), server_);
            ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
            const auto& response = std::get<HttpResponse>(result);

            EXPECT_EQ(response.status, HttpStatus::Forbidden);
            EXPECT_EQ(response.body.find(outsideBody_), std::string::npos);
            EXPECT_EQ(Files::read(outside_ / "secret.txt"), outsideBody_);
        }
    }

    TEST_P(StaticHandlerRootGuardTest, ForbidsAccessThroughOutsideDirectorySymlink) {
        fs::create_directory_symlink(outside_, root_ / "escape");

        const auto result = RequestDispatcher::dispatch(Requests::make(GetParam(), "/escape/secret.txt"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::Forbidden);
        EXPECT_EQ(response.body.find(outsideBody_), std::string::npos);
        EXPECT_EQ(Files::read(outside_ / "secret.txt"), outsideBody_);
        EXPECT_TRUE(fs::is_symlink(fs::symlink_status(root_ / "escape")));
    }

    INSTANTIATE_TEST_SUITE_P(
        Methods,
        StaticHandlerRootGuardTest,
        ::testing::Values(HttpMethod::Get, HttpMethod::Delete),
        [](const ::testing::TestParamInfo<HttpMethod>& info) {
            return info.param == HttpMethod::Get ? std::string("Get") : std::string("Delete");
        }
    );

} // namespace
