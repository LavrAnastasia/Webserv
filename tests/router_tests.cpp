#include "Router.hpp"

#include <gtest/gtest.h>

namespace {

    namespace fs = std::filesystem;

    LocationConfig makeLocation(const std::string& path) {
        LocationConfig location{};
        location.path = path;
        location.allowedMethods = {HttpMethod::Get};
        return location;
    }

    HttpRequest makeRequest(const std::string& path) {
        HttpRequest request{};
        request.method = HttpMethod::Get;
        request.path = path;
        request.target = path;
        request.version = "HTTP/1.1";
        return request;
    }

    class RouterTest : public testing::Test {
    protected:
        ServerConfig server;

        void SetUp() override {
            server.listen = {{"127.0.0.1", 8080}};
            server.root = "/srv/www";
            server.index = "home.html";
            server.clientMaxBodySize = 2 * 1024 * 1024;
            server.locations = {makeLocation("/")};
        }

        std::optional<ResolvedRoute> resolve(const std::string& path) const {
            return Router::resolve(makeRequest(path), server);
        }
    };


    struct MatchCase {
        const char* name;
        const char* path;
        const char* expectedLocation;
    };

    class RouterMatchingTest : public RouterTest, public testing::WithParamInterface<MatchCase> {};

    TEST_P(RouterMatchingTest, SelectsExpectedLocation) {
        server.locations = {
            makeLocation("/api/v1"),
            makeLocation("/foo"),
            makeLocation("/"),
            makeLocation("/api"),
        };

        const auto& test = GetParam();
        const auto route = resolve(test.path);

        if (test.expectedLocation == nullptr) {
            EXPECT_FALSE(route.has_value());
            return;
        }

        ASSERT_TRUE(route.has_value());
        EXPECT_EQ(route->locationPath, test.expectedLocation);
    }

    const MatchCase MATCH_CASES[] = {
        {"Root", "/", "/"},
        {"RootFallback", "/unknown", "/"},
        {"ExactLocation", "/foo", "/foo"},
        {"NestedPath", "/foo/item", "/foo"},
        {"TrailingSlashInRequest", "/foo/", "/foo"},
        {"SimilarPrefix", "/foobar", "/"},
        {"LongestMatch", "/api/v1/users", "/api/v1"},
        {"ExactLongestMatch", "/api/v1", "/api/v1"},
        {"SegmentBoundary", "/api/v10", "/api"},
        {"CaseSensitivePath", "/FOO", "/"},
        {"EmptyPath", "", nullptr},
        {"RelativePath", "foo", nullptr},
    };

    INSTANTIATE_TEST_SUITE_P(
        Paths, RouterMatchingTest, testing::ValuesIn(MATCH_CASES), [](const testing::TestParamInfo<MatchCase>& info) {
            return std::string(info.param.name);
        }
    );

    TEST_F(RouterTest, NoLocationsMeansNoRoute) {
        server.locations.clear();

        EXPECT_FALSE(resolve("/").has_value());
    }

    TEST_F(RouterTest, NoMatchingLocationMeansNoRoute) {
        server.locations = {makeLocation("/foo")};

        EXPECT_FALSE(resolve("/foobar").has_value());
        EXPECT_FALSE(resolve("/other").has_value());
    }

    TEST_F(RouterTest, LongestMatchDoesNotDependOnOrder) {
        const std::array<LocationConfig, 3> locations = {
            makeLocation("/"),
            makeLocation("/api"),
            makeLocation("/api/v1"),
        };

        std::array<std::size_t, 3> order = {0, 1, 2};

        do {
            SCOPED_TRACE(testing::Message() << "order: " << order[0] << ", " << order[1] << ", " << order[2]);

            server.locations = {
                locations[order[0]],
                locations[order[1]],
                locations[order[2]],
            };

            const auto route = resolve("/api/v1/users");

            ASSERT_TRUE(route.has_value());
            EXPECT_EQ(route->locationPath, "/api/v1");
        } while (std::next_permutation(order.begin(), order.end()));
    }

    TEST_F(RouterTest, LocationEndingWithSlashMatchesChildren) {
        server.locations = {
            makeLocation("/"),
            makeLocation("/docs/"),
        };

        const auto route = resolve("/docs/file.txt");

        ASSERT_TRUE(route.has_value());
        EXPECT_EQ(route->locationPath, "/docs/");
    }

    TEST_F(RouterTest, InheritsServerSettings) {
        const auto route = resolve("/index.html");

        ASSERT_TRUE(route.has_value());

        EXPECT_EQ(route->root, fs::path("/srv/www"));
        EXPECT_EQ(route->index, "home.html");
        EXPECT_EQ(route->clientMaxBodySize, 2u * 1024u * 1024u);
        EXPECT_FALSE(route->autoindex);
    }

    TEST_F(RouterTest, LocationOverridesServerSettings) {
        auto location = makeLocation("/assets");
        location.root = "/srv/assets";
        location.index = "listing.html";
        location.clientMaxBodySize = 64 * 1024;

        server.locations = {location};

        const auto route = resolve("/assets/file.txt");

        ASSERT_TRUE(route.has_value());

        EXPECT_EQ(route->root, fs::path("/srv/assets"));
        EXPECT_EQ(route->index, "listing.html");
        EXPECT_EQ(route->clientMaxBodySize, 64u * 1024u);
    }

    TEST_F(RouterTest, ZeroBodyLimitIsAnExplicitOverride) {
        server.locations.front().clientMaxBodySize = 0;

        const auto route = resolve("/");

        ASSERT_TRUE(route.has_value());

        EXPECT_EQ(route->clientMaxBodySize, 0u);
        EXPECT_EQ(route->root, fs::path("/srv/www"));
        EXPECT_EQ(route->index, "home.html");
    }

    TEST_F(RouterTest, InheritsFromServerNotFromLessSpecificLocation) {
        auto parent = makeLocation("/api");
        parent.root = "/srv/parent";
        parent.index = "parent.html";
        parent.clientMaxBodySize = 100;

        const auto child = makeLocation("/api/v1");

        server.locations = {parent, child};

        const auto route = resolve("/api/v1/users");

        ASSERT_TRUE(route.has_value());

        EXPECT_EQ(route->locationPath, "/api/v1");
        EXPECT_EQ(route->root, fs::path("/srv/www"));
        EXPECT_EQ(route->index, "home.html");
        EXPECT_EQ(route->clientMaxBodySize, 2u * 1024u * 1024u);
    }

    TEST_F(RouterTest, UsesSelectedLocationMethodsAndAutoindex) {
        auto selected = makeLocation("/files");
        selected.allowedMethods = {HttpMethod::Get, HttpMethod::Delete};
        selected.autoindex = true;

        server.locations.push_back(selected);

        const auto route = resolve("/files/document.txt");

        ASSERT_TRUE(route.has_value());

        EXPECT_EQ(route->locationPath, "/files");
        EXPECT_TRUE(route->autoindex);
        EXPECT_EQ(route->allowedMethods.size(), 2u);
        EXPECT_TRUE(route->allowedMethods.contains(HttpMethod::Get));
        EXPECT_TRUE(route->allowedMethods.contains(HttpMethod::Delete));
        EXPECT_FALSE(route->allowedMethods.contains(HttpMethod::Post));
    }

    TEST_F(RouterTest, ResolvesErrorPagesAgainstEffectiveRoot) {
        server.errorPages = {
            {HttpStatus::NotFound, "/errors/404.html"},
            {HttpStatus::InternalServerError, "errors/500.html"},
        };

        const auto inherited = resolve("/missing");

        ASSERT_TRUE(inherited.has_value());
        ASSERT_EQ(inherited->errorPages.size(), 2u);

        EXPECT_EQ(inherited->errorPages.at(HttpStatus::NotFound), fs::path("/srv/www/errors/404.html"));
        EXPECT_EQ(inherited->errorPages.at(HttpStatus::InternalServerError), fs::path("/srv/www/errors/500.html"));

        server.locations.front().root = "/srv/custom";

        const auto overridden = resolve("/missing");

        ASSERT_TRUE(overridden.has_value());

        EXPECT_EQ(overridden->errorPages.at(HttpStatus::NotFound), fs::path("/srv/custom/errors/404.html"));
        EXPECT_EQ(overridden->errorPages.at(HttpStatus::InternalServerError), fs::path("/srv/custom/errors/500.html"));
    }

    TEST_F(RouterTest, PreservesRedirectSettings) {
        server.locations.front().redirect = RedirectConfig{HttpStatus::TemporaryRedirect, std::string("/new")};

        const auto route = resolve("/old");

        ASSERT_TRUE(route.has_value());
        ASSERT_TRUE(route->redirect.has_value());

        EXPECT_EQ(route->redirect->status, HttpStatus::TemporaryRedirect);

        ASSERT_TRUE(route->redirect->target.has_value());
        EXPECT_EQ(*route->redirect->target, "/new");
    }

    TEST_F(RouterTest, PreservesUploadSettings) {
        server.locations.front().upload = UploadConfig{fs::path("/srv/uploads")};

        const auto route = resolve("/upload");

        ASSERT_TRUE(route.has_value());
        ASSERT_TRUE(route->upload.has_value());

        EXPECT_EQ(route->upload->uploadPath, fs::path("/srv/uploads"));
    }

    TEST_F(RouterTest, SelectsCgiByExtension) {
        auto& location = server.locations.front();

        location.cgi.emplace(".py", CgiConfig{".py", fs::path("/usr/bin/python3")});
        location.cgi.emplace(".php", CgiConfig{".php", fs::path("/usr/bin/php-cgi")});

        const auto route = resolve("/scripts/app.py");

        ASSERT_TRUE(route.has_value());
        ASSERT_TRUE(route->cgi.has_value());

        EXPECT_EQ(route->cgi->extension, ".py");
        EXPECT_EQ(route->cgi->interpreter, fs::path("/usr/bin/python3"));
    }

    TEST_F(RouterTest, DoesNotSelectCgiForOtherExtensions) {
        server.locations.front().cgi.emplace(".py", CgiConfig{".py", fs::path("/usr/bin/python3")});

        for (const std::string path : {"/script", "/script.txt", "/script.py/file"}) {
            SCOPED_TRACE(path);

            const auto route = resolve(path);

            ASSERT_TRUE(route.has_value());
            EXPECT_FALSE(route->cgi.has_value());
        }
    }

    TEST_F(RouterTest, DoesNotTakeCgiFromAnotherLocation) {
        server.locations.front().cgi.emplace(".py", CgiConfig{".py", fs::path("/usr/bin/python3")});

        server.locations.push_back(makeLocation("/api"));

        const auto route = resolve("/api/app.py");

        ASSERT_TRUE(route.has_value());
        EXPECT_EQ(route->locationPath, "/api");
        EXPECT_FALSE(route->cgi.has_value());
    }

    TEST_F(RouterTest, ResolvesRouteEvenWhenMethodIsNotAllowed) {
        auto request = makeRequest("/");
        request.method = HttpMethod::Post;

        const auto route = Router::resolve(request, server);

        ASSERT_TRUE(route.has_value());
        EXPECT_FALSE(route->allowedMethods.contains(HttpMethod::Post));
    }

} // namespace
