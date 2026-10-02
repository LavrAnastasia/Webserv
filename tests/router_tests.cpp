#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <variant>

#include <gtest/gtest.h>

#include "config/ServerConfig.hpp"
#include "http/RequestDispatcher.hpp"

namespace {

    namespace fs = std::filesystem;

    LocationConfig makeLocation(const std::string& path) {
        LocationConfig location{};
        location.path = path;
        location.allowedMethods = {HttpMethod::Get};
        return location;
    }

    LocationConfig makeRedirectLocation(const std::string& path, const std::string& target) {
        auto location = makeLocation(path);
        location.redirect = RedirectConfig{HttpStatus::Found, target};
        return location;
    }

    HttpRequest
    makeRequest(const std::string& path, HttpMethod method = HttpMethod::Get, const std::string& body = "") {
        HttpRequest request{};
        request.method = method;
        request.path = path;
        request.target = path;
        request.version = "HTTP/1.1";
        request.body = body;
        request.headers.set("Host", "localhost");
        return request;
    }

    class RouterTest : public testing::Test {
    protected:
        fs::path temp_;
        fs::path root_;
        ServerConfig server_{};

        void SetUp() override {
            std::string pattern = (fs::temp_directory_path() / "webserv-router-XXXXXX").string();
            char* directory = ::mkdtemp(pattern.data());
            ASSERT_NE(directory, nullptr);

            temp_ = directory;
            root_ = temp_ / "root";
            fs::create_directories(root_);

            server_.listen = {{"127.0.0.1", 8080}};
            server_.root = root_;
            server_.index = "home.html";
            server_.clientMaxBodySize = 8;
            server_.locations = {makeLocation("/")};
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

        void expectHeader(const HttpResponse& response, const std::string& name, const std::string& expected) const {
            const auto value = response.headers.get(name);
            ASSERT_TRUE(value.has_value()) << name;
            EXPECT_EQ(*value, expected) << name;
        }

        void expectResponse(
            const HttpRequest& request, HttpStatus status, const std::optional<std::string>& body = std::nullopt
        ) const {
            const auto result = RequestDispatcher::dispatch(request, server_);
            ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
            const auto& response = std::get<HttpResponse>(result);

            EXPECT_EQ(response.status, status);
            if (body.has_value()) {
                EXPECT_EQ(response.body, *body);
            }
        }

        void expectRedirect(
            const std::string& path, const std::string& target, HttpStatus status = HttpStatus::Found
        ) const {
            const auto result = RequestDispatcher::dispatch(makeRequest(path), server_);
            ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
            const auto& response = std::get<HttpResponse>(result);

            EXPECT_EQ(response.status, status);
            expectHeader(response, "Location", target);
        }
    };

    struct MatchCase {
        const char* name;
        const char* path;
        const char* expectedTarget;
    };

    class RouterMatchingTest : public RouterTest, public testing::WithParamInterface<MatchCase> {};

    TEST_P(RouterMatchingTest, SelectsExpectedLocation) {
        server_.locations = {
            makeRedirectLocation("/api/v1", "/selected/api/v1"),
            makeRedirectLocation("/foo", "/selected/foo"),
            makeRedirectLocation("/", "/selected/root"),
            makeRedirectLocation("/api", "/selected/api"),
        };

        const auto& test = GetParam();
        if (test.expectedTarget == nullptr) {
            expectResponse(makeRequest(test.path), HttpStatus::NotFound);
        } else {
            expectRedirect(test.path, test.expectedTarget);
        }
    }

    const MatchCase MATCH_CASES[] = {
        {"Root", "/", "/selected/root"},
        {"RootFallback", "/unknown", "/selected/root"},
        {"ExactLocation", "/foo", "/selected/foo"},
        {"NestedPath", "/foo/item", "/selected/foo"},
        {"TrailingSlashInRequest", "/foo/", "/selected/foo"},
        {"SimilarPrefix", "/foobar", "/selected/root"},
        {"LongestMatch", "/api/v1/users", "/selected/api/v1"},
        {"ExactLongestMatch", "/api/v1", "/selected/api/v1"},
        {"SegmentBoundary", "/api/v10", "/selected/api"},
        {"CaseSensitivePath", "/FOO", "/selected/root"},
        {"EmptyPath", "", nullptr},
        {"RelativePath", "foo", nullptr},
    };

    INSTANTIATE_TEST_SUITE_P(
        Paths, RouterMatchingTest, testing::ValuesIn(MATCH_CASES), [](const testing::TestParamInfo<MatchCase>& info) {
            return std::string(info.param.name);
        }
    );

    TEST_F(RouterTest, Returns404WhenThereAreNoLocations) {
        writeFile(root_ / "home.html", "existing index");
        server_.locations.clear();

        expectResponse(makeRequest("/"), HttpStatus::NotFound);
    }

    TEST_F(RouterTest, Returns404WhenNoLocationMatches) {
        server_.locations = {makeLocation("/foo")};

        for (const std::string path : {"/foobar", "/other"}) {
            SCOPED_TRACE(path);
            writeFile(root_ / fs::path(path).relative_path(), "existing file");
            expectResponse(makeRequest(path), HttpStatus::NotFound);
        }
    }

    TEST_F(RouterTest, LongestMatchDoesNotDependOnOrder) {
        const std::array<LocationConfig, 3> locations = {
            makeRedirectLocation("/", "/selected/root"),
            makeRedirectLocation("/api", "/selected/api"),
            makeRedirectLocation("/api/v1", "/selected/api/v1"),
        };
        std::array<std::size_t, 3> order = {0, 1, 2};

        do {
            SCOPED_TRACE(testing::Message() << "order: " << order[0] << ", " << order[1] << ", " << order[2]);
            server_.locations = {
                locations[order[0]],
                locations[order[1]],
                locations[order[2]],
            };

            expectRedirect("/api/v1/users", "/selected/api/v1");
        } while (std::next_permutation(order.begin(), order.end()));
    }

    TEST_F(RouterTest, LocationEndingWithSlashMatchesChildren) {
        server_.locations = {
            makeRedirectLocation("/", "/selected/root"),
            makeRedirectLocation("/docs/", "/selected/docs"),
        };

        expectRedirect("/docs/file.txt", "/selected/docs");
    }

    TEST_F(RouterTest, InheritsServerRootAndIndex) {
        writeFile(root_ / "home.html", "server index");
        writeFile(root_ / "index.html", "wrong index");

        expectResponse(makeRequest("/"), HttpStatus::OK, "server index");
    }

    TEST_F(RouterTest, InheritsServerBodyLimit) {
        writeFile(root_ / "file.txt", "contents");

        expectResponse(makeRequest("/file.txt", HttpMethod::Get, "12345678"), HttpStatus::OK, "contents");
        expectResponse(makeRequest("/file.txt", HttpMethod::Get, "123456789"), HttpStatus::PayloadTooLarge);
    }

    TEST_F(RouterTest, AutoindexIsDisabledByDefault) {
        fs::create_directories(root_ / "docs");

        expectResponse(makeRequest("/docs/"), HttpStatus::Forbidden);
    }

    TEST_F(RouterTest, LocationOverridesServerRootAndIndex) {
        const fs::path customRoot = temp_ / "custom";
        auto location = makeLocation("/assets");
        location.root = customRoot;
        location.index = "listing.html";
        server_.locations = {location};

        // The full request path is resolved beneath the configured root.
        writeFile(root_ / "assets" / "home.html", "server root and index");
        writeFile(root_ / "assets" / "listing.html", "server root");
        writeFile(customRoot / "assets" / "home.html", "server index");
        writeFile(customRoot / "assets" / "listing.html", "location root and index");

        expectResponse(makeRequest("/assets/"), HttpStatus::OK, "location root and index");
    }

    TEST_F(RouterTest, LocationCanReduceBodyLimit) {
        writeFile(root_ / "file.txt", "contents");
        server_.locations.front().clientMaxBodySize = 4;

        expectResponse(makeRequest("/file.txt", HttpMethod::Get, "1234"), HttpStatus::OK, "contents");
        expectResponse(makeRequest("/file.txt", HttpMethod::Get, "12345"), HttpStatus::PayloadTooLarge);
    }

    TEST_F(RouterTest, LocationCanIncreaseBodyLimit) {
        writeFile(root_ / "file.txt", "contents");
        server_.locations.front().clientMaxBodySize = 16;

        expectResponse(makeRequest("/file.txt", HttpMethod::Get, std::string(16, 'x')), HttpStatus::OK, "contents");
        expectResponse(makeRequest("/file.txt", HttpMethod::Get, std::string(17, 'x')), HttpStatus::PayloadTooLarge);
    }

    TEST_F(RouterTest, ZeroBodyLimitIsAnExplicitOverride) {
        writeFile(root_ / "file.txt", "contents");
        server_.locations.front().clientMaxBodySize = 0;

        expectResponse(makeRequest("/file.txt"), HttpStatus::OK, "contents");
        expectResponse(makeRequest("/file.txt", HttpMethod::Get, "x"), HttpStatus::PayloadTooLarge);
    }

    TEST_F(RouterTest, InheritsFromServerNotFromLessSpecificLocation) {
        const fs::path parentRoot = temp_ / "parent";
        auto parent = makeLocation("/api");
        parent.root = parentRoot;
        parent.index = "parent.html";
        parent.clientMaxBodySize = 1;
        server_.locations = {parent, makeLocation("/api/v1")};

        writeFile(root_ / "api/v1/home.html", "server settings");
        writeFile(root_ / "api/v1/parent.html", "parent index");
        writeFile(parentRoot / "api/v1/home.html", "parent root");
        writeFile(parentRoot / "api/v1/parent.html", "parent root and index");

        expectResponse(makeRequest("/api/v1/", HttpMethod::Get, "12345678"), HttpStatus::OK, "server settings");
        expectResponse(makeRequest("/api/v1/", HttpMethod::Get, "123456789"), HttpStatus::PayloadTooLarge);
    }

    TEST_F(RouterTest, UsesSelectedLocationMethods) {
        auto selected = makeLocation("/files");
        selected.allowedMethods = {HttpMethod::Get, HttpMethod::Delete};
        server_.locations.push_back(selected);
        writeFile(root_ / "files/document.txt", "document");

        const auto result = RequestDispatcher::dispatch(makeRequest("/files/document.txt", HttpMethod::Post), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::MethodNotAllowed);
        EXPECT_TRUE(response.headers.has("Allow", "GET"));
        EXPECT_TRUE(response.headers.has("Allow", "DELETE"));
        EXPECT_FALSE(response.headers.has("Allow", "POST"));

        expectResponse(makeRequest("/files/document.txt"), HttpStatus::OK, "document");
        expectResponse(makeRequest("/files/document.txt", HttpMethod::Delete), HttpStatus::NoContent, "");
        expectResponse(makeRequest("/files/document.txt"), HttpStatus::NotFound);
    }

    TEST_F(RouterTest, UsesSelectedLocationAutoindex) {
        auto selected = makeLocation("/files");
        selected.autoindex = true;
        server_.locations.push_back(selected);
        writeFile(root_ / "files/document.txt", "document");

        const auto result = RequestDispatcher::dispatch(makeRequest("/files/"), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        ASSERT_EQ(response.status, HttpStatus::OK);
        expectHeader(response, "Content-Type", "text/html; charset=utf-8");
        EXPECT_NE(response.body.find("<a href=\"document.txt\">document.txt</a>"), std::string::npos);
    }

    TEST_F(RouterTest, LoadsErrorPagesFromEffectiveRoot) {
        const fs::path customRoot = temp_ / "custom";
        server_.errorPages = {
            {HttpStatus::NotFound, "/errors/404.html"},
            {HttpStatus::InternalServerError, "errors/500.html"},
        };
        auto& location = server_.locations.front();
        location.allowedMethods = {HttpMethod::Get, HttpMethod::Post};
        location.upload = UploadConfig{fs::path("not-a-directory")};

        writeFile(root_ / "errors/404.html", "server 404");
        writeFile(root_ / "errors/500.html", "server 500");
        writeFile(customRoot / "errors/404.html", "location 404");
        writeFile(customRoot / "errors/500.html", "location 500");

        // An existing regular file used as an upload directory causes a 500 response.
        writeFile(root_ / "not-a-directory", "file");
        writeFile(customRoot / "not-a-directory", "file");

        for (const bool overrideRoot : {false, true}) {
            SCOPED_TRACE(overrideRoot ? "location root" : "server root");
            if (overrideRoot) {
                location.root = customRoot;
            }
            const std::string prefix = overrideRoot ? "location " : "server ";

            expectResponse(makeRequest("/missing"), HttpStatus::NotFound, prefix + "404");
            expectResponse(
                makeRequest("/upload/file.txt", HttpMethod::Post, "data"),
                HttpStatus::InternalServerError,
                prefix + "500"
            );
        }
    }

    TEST_F(RouterTest, ReturnsConfiguredRedirect) {
        server_.locations.front().redirect = RedirectConfig{HttpStatus::TemporaryRedirect, std::string("/new")};

        expectRedirect("/old", "/new", HttpStatus::TemporaryRedirect);
    }

    TEST_F(RouterTest, UploadsToConfiguredDirectory) {
        fs::create_directories(root_ / "incoming");
        auto location = makeLocation("/upload");
        location.allowedMethods = {HttpMethod::Get, HttpMethod::Post};
        location.upload = UploadConfig{fs::path("incoming")};
        server_.locations.push_back(location);

        const std::string body = "payload";
        const auto result =
            RequestDispatcher::dispatch(makeRequest("/upload/file.txt", HttpMethod::Post, body), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        ASSERT_EQ(response.status, HttpStatus::Created);
        expectHeader(response, "Location", "/upload/file.txt");
        EXPECT_TRUE(response.body.empty());
        ASSERT_TRUE(fs::is_regular_file(root_ / "incoming/file.txt"));
        EXPECT_EQ(readFile(root_ / "incoming/file.txt"), body);
    }

    TEST_F(RouterTest, SelectsCgiByExtension) {
        auto& location = server_.locations.front();
        location.allowedMethods = {HttpMethod::Get, HttpMethod::Post};
        location.cgi.emplace(".py", CgiConfig{".py", fs::path("/usr/bin/python3")});
        location.cgi.emplace(".php", CgiConfig{".php", fs::path("/usr/bin/php-cgi")});

        struct CgiCase {
            const char* filename;
            const char* interpreter;
        };
        const CgiCase cases[] = {
            {"app.py", "/usr/bin/python3"},
            {"app.php", "/usr/bin/php-cgi"},
        };

        for (const auto& test : cases) {
            SCOPED_TRACE(test.filename);
            const fs::path script = root_ / "scripts" / test.filename;
            writeFile(script, "script contents");

            const std::string path = std::string("/scripts/") + test.filename;
            const auto result = RequestDispatcher::dispatch(makeRequest(path, HttpMethod::Post, "input"), server_);
            ASSERT_TRUE(std::holds_alternative<CgiRequest>(result));
            const auto& cgi = std::get<CgiRequest>(result);

            EXPECT_EQ(cgi.interpreter, fs::path(test.interpreter));
            EXPECT_EQ(cgi.script, fs::canonical(script));
            EXPECT_EQ(cgi.body, "input");
        }
    }

    TEST_F(RouterTest, ServesOtherExtensionsAsStaticFiles) {
        server_.locations.front().cgi.emplace(".py", CgiConfig{".py", fs::path("/usr/bin/python3")});

        for (const std::string path : {"/script", "/script.txt", "/script.py/file"}) {
            SCOPED_TRACE(path);
            const std::string body = "static contents: " + path;
            writeFile(root_ / fs::path(path).relative_path(), body);

            expectResponse(makeRequest(path), HttpStatus::OK, body);
        }
    }

    TEST_F(RouterTest, DoesNotTakeCgiFromAnotherLocation) {
        server_.locations.front().cgi.emplace(".py", CgiConfig{".py", fs::path("/usr/bin/python3")});
        server_.locations.push_back(makeLocation("/api"));
        writeFile(root_ / "api/app.py", "served as static content");

        expectResponse(makeRequest("/api/app.py"), HttpStatus::OK, "served as static content");
    }

    TEST_F(RouterTest, Returns405WithAllowWhenMethodIsNotAllowed) {
        const auto result = RequestDispatcher::dispatch(makeRequest("/", HttpMethod::Post), server_);
        ASSERT_TRUE(std::holds_alternative<HttpResponse>(result));
        const auto& response = std::get<HttpResponse>(result);

        EXPECT_EQ(response.status, HttpStatus::MethodNotAllowed);
        expectHeader(response, "Allow", "GET");
    }

} // namespace
