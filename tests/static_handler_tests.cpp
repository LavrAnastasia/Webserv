#include <fstream>
#include <gtest/gtest.h>

#include "StaticHandler.hpp"

namespace {

    namespace fs = std::filesystem;

    class StaticHandlerTest : public ::testing::Test {
    protected:
        fs::path temp_;
        fs::path root_;
        fs::path outside_;
        ResolvedRoute route_{};

        const std::string outsideBody_ = "outside secret\n";

        void SetUp() override {
            std::string pattern = (fs::temp_directory_path() / "webserv-static-XXXXXX").string();

            char* directory = ::mkdtemp(pattern.data());
            ASSERT_NE(directory, nullptr);

            temp_ = directory;
            root_ = temp_ / "root";

            // Similar prefix must not make this directory part of root.
            outside_ = temp_ / "root-other";

            fs::create_directories(root_);
            fs::create_directories(outside_);
            writeFile(outside_ / "secret.txt", outsideBody_);

            route_.locationPath = "/";
            route_.root = root_;
            route_.index = "index.html";
            route_.allowedMethods = {HttpMethod::Get, HttpMethod::Delete};
            route_.autoindex = false;
        }

        void TearDown() override {
            if (temp_.empty()) {
                return;
            }

            std::error_code error;
            fs::remove_all(temp_, error);
            EXPECT_FALSE(error) << error.message();
        }

        void writeFile(const fs::path& path, const std::string& body) const {
            fs::create_directories(path.parent_path());

            std::ofstream file;
            file.exceptions(std::ios::failbit | std::ios::badbit);
            file.open(path, std::ios::binary);
            file.write(body.data(), static_cast<std::streamsize>(body.size()));
            file.close();
        }

        std::string readFile(const fs::path& path) const {
            std::ifstream file;
            file.exceptions(std::ios::failbit | std::ios::badbit);
            file.open(path, std::ios::binary);

            return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }

        HttpRequest requestFor(const std::string& path, HttpMethod method = HttpMethod::Get) const {
            HttpRequest request{};
            request.method = method;
            request.target = path;
            request.path = path;
            request.version = "HTTP/1.1";
            request.headers.set("Host", "localhost");
            return request;
        }

        void expectHeader(const HttpResponse& response, const std::string& name, const std::string& expected) const {
            const auto value = response.headers.get(name);
            ASSERT_TRUE(value.has_value()) << name;
            EXPECT_EQ(*value, expected) << name;
        }
    };

    // File contents

    struct FileContentsCase {
        const char* name;
        std::string body;
    };

    class StaticHandlerFileContentsTest : public StaticHandlerTest,
                                          public ::testing::WithParamInterface<FileContentsCase> {};

    TEST_P(StaticHandlerFileContentsTest, ReturnsExactFileContents) {
        const auto& test = GetParam();
        writeFile(root_ / "payload.bin", test.body);

        const auto response = StaticHandler::handle(requestFor("/payload.bin"), route_);

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
        const auto response = StaticHandler::handle(requestFor("/missing.txt"), route_);

        EXPECT_EQ(response.status, HttpStatus::NotFound);
        EXPECT_FALSE(response.body.empty());
    }

    TEST_F(StaticHandlerTest, Returns400ForInvalidRequestPath) {
        for (const std::string path : {"", "relative.txt"}) {
            SCOPED_TRACE(path);

            const auto response = StaticHandler::handle(requestFor(path), route_);

            EXPECT_EQ(response.status, HttpStatus::BadRequest);
        }
    }

    // Index

    TEST_F(StaticHandlerTest, ReturnsIndexForRootDirectory) {
        const std::string body = "<h1>Home</h1>\n";
        writeFile(root_ / "index.html", body);

        const auto response = StaticHandler::handle(requestFor("/"), route_);

        ASSERT_EQ(response.status, HttpStatus::OK);
        EXPECT_EQ(response.body, body);
        expectHeader(response, "Content-Type", "text/html; charset=utf-8");
    }

    TEST_F(StaticHandlerTest, UsesConfiguredNestedIndexBeforeAutoindex) {
        route_.index = "home.html";
        route_.autoindex = true;

        const std::string body = "<h1>Documentation</h1>\n";
        writeFile(root_ / "docs" / "home.html", body);
        writeFile(root_ / "docs" / "index.html", "wrong index");

        const auto response = StaticHandler::handle(requestFor("/docs/"), route_);

        ASSERT_EQ(response.status, HttpStatus::OK);
        EXPECT_EQ(response.body, body);
    }

    TEST_F(StaticHandlerTest, RedirectsDirectoryAndPreservesQuery) {
        fs::create_directories(root_ / "my docs");

        auto request = requestFor("/my docs");
        request.query = "sort=name&order=asc";
        request.target = "/my%20docs?" + request.query;

        const auto response = StaticHandler::handle(request, route_);

        EXPECT_EQ(response.status, HttpStatus::MovedPermanently);
        expectHeader(response, "Location", "/my%20docs/?sort=name&order=asc");
    }

    TEST_F(StaticHandlerTest, ForbidsDirectoryWithoutIndexWhenAutoindexIsOff) {
        fs::create_directories(root_ / "docs");

        for (const std::string index : {"index.html", ""}) {
            SCOPED_TRACE(index);
            route_.index = index;

            const auto response = StaticHandler::handle(requestFor("/docs/"), route_);

            EXPECT_EQ(response.status, HttpStatus::Forbidden);
        }
    }

    TEST_F(StaticHandlerTest, ForbidsIndexThatIsDirectoryEvenWithAutoindex) {
        route_.autoindex = true;
        fs::create_directories(root_ / "index.html");

        const auto response = StaticHandler::handle(requestFor("/"), route_);

        EXPECT_EQ(response.status, HttpStatus::Forbidden);
    }

    // Autoindex

    TEST_F(StaticHandlerTest, AutoindexSortsDirectoriesBeforeFiles) {
        route_.autoindex = true;

        writeFile(root_ / "z.txt", "z");
        fs::create_directories(root_ / "z-dir");
        writeFile(root_ / "a.txt", "a");
        fs::create_directories(root_ / "a-dir");

        const auto response = StaticHandler::handle(requestFor("/"), route_);

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
        route_.index.clear();
        route_.autoindex = true;
        writeFile(root_ / "docs" / "readme.txt", "documentation");

        const auto response = StaticHandler::handle(requestFor("/docs/"), route_);

        ASSERT_EQ(response.status, HttpStatus::OK);
        EXPECT_NE(response.body.find("<a href=\"../\">../</a>"), std::string::npos);
        EXPECT_NE(response.body.find("<a href=\"readme.txt\">readme.txt</a>"), std::string::npos);
    }

    TEST_F(StaticHandlerTest, AutoindexEscapesHtmlAndEncodesLinks) {
        route_.autoindex = true;
        writeFile(root_ / "<docs>" / "a & <b>.txt", "contents");

        const auto response = StaticHandler::handle(requestFor("/<docs>/"), route_);

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

    // DELETE

    TEST_F(StaticHandlerTest, DeletesFileAndReturns204WithEmptyBody) {
        writeFile(root_ / "delete-me.txt", "temporary");

        const auto response = StaticHandler::handle(requestFor("/delete-me.txt", HttpMethod::Delete), route_);

        ASSERT_EQ(response.status, HttpStatus::NoContent);
        EXPECT_TRUE(response.body.empty());
        EXPECT_FALSE(fs::exists(root_ / "delete-me.txt"));

        const auto repeated = StaticHandler::handle(requestFor("/delete-me.txt", HttpMethod::Delete), route_);

        EXPECT_EQ(repeated.status, HttpStatus::NotFound);
    }

    TEST_F(StaticHandlerTest, DeleteForbidsDirectoriesAndRoot) {
        writeFile(root_ / "docs" / "keep.txt", "keep");

        for (const std::string path : {"/", "/docs", "/docs/"}) {
            SCOPED_TRACE(path);

            const auto response = StaticHandler::handle(requestFor(path, HttpMethod::Delete), route_);

            EXPECT_EQ(response.status, HttpStatus::Forbidden);
            EXPECT_TRUE(fs::is_directory(root_));
            EXPECT_TRUE(fs::is_directory(root_ / "docs"));
            EXPECT_EQ(readFile(root_ / "docs" / "keep.txt"), "keep");
        }
    }

    TEST_F(StaticHandlerTest, DeleteRemovesSymlinkButPreservesOutsideTarget) {
        const fs::path link = root_ / "link.txt";
        fs::create_symlink(outside_ / "secret.txt", link);

        const auto response = StaticHandler::handle(requestFor("/link.txt", HttpMethod::Delete), route_);

        ASSERT_EQ(response.status, HttpStatus::NoContent);
        EXPECT_TRUE(response.body.empty());

        // symlink_status checks the link itself, including dangling links.
        EXPECT_FALSE(fs::exists(fs::symlink_status(link)));
        EXPECT_EQ(readFile(outside_ / "secret.txt"), outsideBody_);
    }

    TEST_F(StaticHandlerTest, DeleteRemovesDanglingSymlink) {
        const fs::path link = root_ / "broken.txt";
        fs::create_symlink(outside_ / "missing.txt", link);
        ASSERT_TRUE(fs::is_symlink(fs::symlink_status(link)));

        const auto response = StaticHandler::handle(requestFor("/broken.txt", HttpMethod::Delete), route_);

        EXPECT_EQ(response.status, HttpStatus::NoContent);
        EXPECT_FALSE(fs::exists(fs::symlink_status(link)));
    }

    // Root containment

    TEST_F(StaticHandlerTest, GetAllowsSymlinkWhoseTargetIsInsideRoot) {
        writeFile(root_ / "data" / "file.txt", "inside");
        fs::create_symlink(root_ / "data" / "file.txt", root_ / "link.txt");

        const auto response = StaticHandler::handle(requestFor("/link.txt"), route_);

        ASSERT_EQ(response.status, HttpStatus::OK);
        EXPECT_EQ(response.body, "inside");
    }

    TEST_F(StaticHandlerTest, GetForbidsFileSymlinkPointingOutsideRoot) {
        fs::create_symlink(outside_ / "secret.txt", root_ / "link.txt");

        const auto response = StaticHandler::handle(requestFor("/link.txt"), route_);

        EXPECT_EQ(response.status, HttpStatus::Forbidden);
        EXPECT_EQ(response.body.find(outsideBody_), std::string::npos);
        EXPECT_EQ(readFile(outside_ / "secret.txt"), outsideBody_);
    }

    TEST_F(StaticHandlerTest, IndexCannotEscapeRoot) {
        route_.autoindex = true;
        fs::create_symlink(outside_ / "secret.txt", root_ / "index.html");

        const std::string indexes[] = {
            "index.html",
            "../root-other/secret.txt",
            (outside_ / "secret.txt").string(),
        };

        for (const auto& index : indexes) {
            SCOPED_TRACE(index);
            route_.index = index;

            const auto response = StaticHandler::handle(requestFor("/"), route_);

            EXPECT_EQ(response.status, HttpStatus::Forbidden);
            EXPECT_EQ(response.body.find(outsideBody_), std::string::npos);
        }

        EXPECT_EQ(readFile(outside_ / "secret.txt"), outsideBody_);
    }

    class StaticHandlerRootGuardTest : public StaticHandlerTest, public ::testing::WithParamInterface<HttpMethod> {};

    TEST_P(StaticHandlerRootGuardTest, ForbidsDotDotEscapeToSimilarRootPrefix) {
        fs::create_directories(root_ / "docs");

        for (const std::string path : {"/../root-other/secret.txt", "/docs/../../root-other/secret.txt"}) {
            SCOPED_TRACE(path);

            const auto response = StaticHandler::handle(requestFor(path, GetParam()), route_);

            EXPECT_EQ(response.status, HttpStatus::Forbidden);
            EXPECT_EQ(response.body.find(outsideBody_), std::string::npos);
            EXPECT_EQ(readFile(outside_ / "secret.txt"), outsideBody_);
        }
    }

    TEST_P(StaticHandlerRootGuardTest, ForbidsAccessThroughOutsideDirectorySymlink) {
        fs::create_directory_symlink(outside_, root_ / "escape");

        const auto response = StaticHandler::handle(requestFor("/escape/secret.txt", GetParam()), route_);

        EXPECT_EQ(response.status, HttpStatus::Forbidden);
        EXPECT_EQ(response.body.find(outsideBody_), std::string::npos);
        EXPECT_EQ(readFile(outside_ / "secret.txt"), outsideBody_);
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
