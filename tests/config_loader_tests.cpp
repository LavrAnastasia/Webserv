#include "config/ConfigError.hpp"
#include "config/ConfigLoader.hpp"
#include "includes/Files.hpp"
#include "includes/TempDirectory.hpp"

#include <gtest/gtest.h>

#include <filesystem>

namespace {

    namespace fs = std::filesystem;

    constexpr std::size_t CONFIG_FILE_LIMIT = 1024 * 1024;

    const std::string SERVER_DIRECTIVES = "listen 127.0.0.1:8080;\n"
                                          "root ./public;\n";

    const std::string LOCATION = "location / { methods GET; }\n";

    std::string makeConfig(const std::string& directives = SERVER_DIRECTIVES, const std::string& locations = LOCATION) {
        return "server {\n" + directives + locations + "}\n";
    }

    std::string withListen(const std::string& endpoint) {
        return makeConfig("listen " + endpoint + ";\nroot ./public;\n");
    }

    class ConfigLoaderTest : public testing::Test {
    protected:
        TempDirectory tempDir_{"webserv-config-test"};

        std::string writeConfig(const std::string& source) {
            const fs::path path = tempDir_.path() / "test.conf";
            Files::write(path, source);
            return path.string();
        }

        Configuration loadText(const std::string& source) { return ConfigLoader::load(writeConfig(source)); }
    };

    TEST_F(ConfigLoaderTest, LoadsMinimalConfigAndDefaults) {
        const auto config = loadText(makeConfig());

        ASSERT_EQ(config.servers.size(), 1u);

        const auto& server = config.servers.front();

        ASSERT_EQ(server.listen.size(), 1u);
        EXPECT_EQ(server.listen.front().host, "127.0.0.1");
        EXPECT_EQ(server.listen.front().port, 8080);

        EXPECT_EQ(server.root, fs::path("./public"));
        EXPECT_EQ(server.index, "index.html");
        EXPECT_EQ(server.clientMaxBodySize, 1024u * 1024u);
        EXPECT_TRUE(server.errorPages.empty());

        ASSERT_EQ(server.locations.size(), 1u);

        const auto& location = server.locations.front();

        EXPECT_EQ(location.path, "/");
        ASSERT_EQ(location.allowedMethods.size(), 1u);
        EXPECT_TRUE(location.allowedMethods.contains(HttpMethod::Get));
        EXPECT_FALSE(location.autoindex);
        EXPECT_FALSE(location.root.has_value());
        EXPECT_FALSE(location.index.has_value());
        EXPECT_FALSE(location.clientMaxBodySize.has_value());

        EXPECT_FALSE(location.redirect.has_value());
        EXPECT_FALSE(location.upload.has_value());
        EXPECT_TRUE(location.cgi.empty());
    }

    TEST_F(ConfigLoaderTest, LoadsExplicitServerAndLocationSettings) {
        const auto config = loadText(R"(
        server {
            listen 127.0.0.1:8080;
            root /srv/www;
            index home.html;
            client_max_body_size 2m;
            error_page 404 500 /error.html;

            location /assets {
                root /srv/assets;
                index listing.html;
                client_max_body_size 64k;
                autoindex on;
                methods GET DELETE;
            }
        }
    )");

        ASSERT_EQ(config.servers.size(), 1u);

        const auto& server = config.servers.front();

        EXPECT_EQ(server.root, fs::path("/srv/www"));
        EXPECT_EQ(server.index, "home.html");
        EXPECT_EQ(server.clientMaxBodySize, 2u * 1024u * 1024u);

        ASSERT_EQ(server.errorPages.size(), 2u);
        EXPECT_EQ(server.errorPages.at(HttpStatus::NotFound), fs::path("/error.html"));
        EXPECT_EQ(server.errorPages.at(HttpStatus::InternalServerError), fs::path("/error.html"));

        ASSERT_EQ(server.locations.size(), 1u);

        const auto& location = server.locations.front();

        EXPECT_EQ(location.path, "/assets");

        ASSERT_TRUE(location.root.has_value());
        EXPECT_EQ(*location.root, fs::path("/srv/assets"));

        ASSERT_TRUE(location.index.has_value());
        EXPECT_EQ(*location.index, "listing.html");

        ASSERT_TRUE(location.clientMaxBodySize.has_value());
        EXPECT_EQ(*location.clientMaxBodySize, 64u * 1024u);

        EXPECT_TRUE(location.autoindex);
        EXPECT_EQ(location.allowedMethods.size(), 2u);
        EXPECT_TRUE(location.allowedMethods.contains(HttpMethod::Get));
        EXPECT_TRUE(location.allowedMethods.contains(HttpMethod::Delete));
    }

    TEST_F(ConfigLoaderTest, LoadsMultipleServersAndListenAddresses) {
        const auto config = loadText(
            makeConfig(
                "listen localhost:8080;\n"
                "listen 127.0.0.1:8081;\n"
                "root ./first;\n"
            ) +
            makeConfig(
                "listen 0.0.0.0:9000;\n"
                "root ./second;\n"
            )
        );

        ASSERT_EQ(config.servers.size(), 2u);

        const auto& first = config.servers[0];
        const auto& second = config.servers[1];

        ASSERT_EQ(first.listen.size(), 2u);

        EXPECT_EQ(first.listen[0].host, "127.0.0.1");
        EXPECT_EQ(first.listen[0].port, 8080);
        EXPECT_EQ(first.listen[1].port, 8081);
        EXPECT_EQ(first.root, fs::path("./first"));

        ASSERT_EQ(second.listen.size(), 1u);
        EXPECT_EQ(second.listen[0].host, "0.0.0.0");
        EXPECT_EQ(second.listen[0].port, 9000);
        EXPECT_EQ(second.root, fs::path("./second"));
    }

    TEST_F(ConfigLoaderTest, IgnoresCommentsAndWhitespace) {
        const auto config = loadText(R"(
        # Comment before the server
        server {
            listen 127.0.0.1:8080; # Inline comment
            root ./public;

            # Symbols inside a comment: { } ;
            location / {
                methods GET;
            }
        }
        # Comment after the server
    )");

        ASSERT_EQ(config.servers.size(), 1u);
        EXPECT_EQ(config.servers.front().root, fs::path("./public"));
        ASSERT_EQ(config.servers.front().locations.size(), 1u);
        EXPECT_EQ(config.servers.front().locations.front().path, "/");
    }

    class ConfigLoaderPortTest : public ConfigLoaderTest, public testing::WithParamInterface<unsigned int> {};

    TEST_P(ConfigLoaderPortTest, AcceptsValidPort) {
        const auto config = loadText(withListen("127.0.0.1:" + std::to_string(GetParam())));

        ASSERT_EQ(config.servers.size(), 1u);
        ASSERT_EQ(config.servers.front().listen.size(), 1u);

        EXPECT_EQ(config.servers.front().listen.front().port, GetParam());
    }

    INSTANTIATE_TEST_SUITE_P(Ports, ConfigLoaderPortTest, testing::Values(1u, 8080u, 65535u));

    struct InvalidConfigCase {
        const char* name;
        std::string source;
    };

    class ConfigLoaderInvalidTest : public ConfigLoaderTest, public testing::WithParamInterface<InvalidConfigCase> {};

    TEST_P(ConfigLoaderInvalidTest, RejectsInvalidConfig) {
        EXPECT_THROW(loadText(GetParam().source), ConfigError);
    }

    const InvalidConfigCase INVALID_CONFIGS[] = {
        {"EmptyConfig", ""},
        {"CommentsOnly", "# No configuration here\n"},

        {"UnclosedServer", "server {\n" + SERVER_DIRECTIVES + LOCATION},
        {"UnclosedLocation", "server {\n" + SERVER_DIRECTIVES + "location / { methods GET;\n"},
        {"UnexpectedClosingBrace", makeConfig() + "}\n"},
        {"MissingSemicolon", makeConfig(SERVER_DIRECTIVES, "location / { methods GET }\n")},

        {"UnknownBlock", "http {}\n"},
        {"UnknownServerDirective", makeConfig(SERVER_DIRECTIVES + "unknown_directive on;\n")},
        {"UnknownLocationDirective",
         makeConfig(SERVER_DIRECTIVES, "location / { methods GET; unknown_directive on; }\n")},

        {"MissingListen", makeConfig("root ./public;\n")},
        {"MissingRoot", makeConfig("listen 127.0.0.1:8080;\n")},
        {"MissingLocation", makeConfig(SERVER_DIRECTIVES, "")},
        {"MissingMethods", makeConfig(SERVER_DIRECTIVES, "location / {}\n")},

        {"ZeroPort", withListen("127.0.0.1:0")},
        {"PortTooLarge", withListen("127.0.0.1:65536")},
        {"NegativePort", withListen("127.0.0.1:-1")},
        {"NonNumericPort", withListen("127.0.0.1:abc")},
        {"PortWithTrailingText", withListen("127.0.0.1:8080abc")},
        {"PortOverflow", withListen("127.0.0.1:999999999999999999999")},
        {"MissingPort", withListen("127.0.0.1:")},
        {"MissingHost", withListen(":8080")},
        {"InvalidIpv4", withListen("256.0.0.1:8080")},

        {"DuplicateRoot", makeConfig(SERVER_DIRECTIVES + "root ./other;\n")},
        {"DuplicateEndpoint", makeConfig(SERVER_DIRECTIVES + "listen localhost:8080;\n")},
        {"DuplicateLocation", makeConfig(SERVER_DIRECTIVES, LOCATION + LOCATION)},

        {"MethodsInServerBlock", makeConfig(SERVER_DIRECTIVES + "methods GET;\n")},
        {"TooManyIndexArguments", makeConfig(SERVER_DIRECTIVES + "index first.html second.html;\n")},
        {"IndexWithoutArgument", makeConfig(SERVER_DIRECTIVES + "index;\n")},
        {"ErrorPageWithoutLeadingSlash", makeConfig(SERVER_DIRECTIVES + "error_page 404 errors/404.html;\n")},

        {"UnsupportedMethod", makeConfig(SERVER_DIRECTIVES, "location / { methods PUT; }\n")},
        {"DuplicateMethod", makeConfig(SERVER_DIRECTIVES, "location / { methods GET GET; }\n")},
        {"InvalidLocationPath", makeConfig(SERVER_DIRECTIVES, "location assets { methods GET; }\n")},
        {"InvalidAutoindex", makeConfig(SERVER_DIRECTIVES, "location / { methods GET; autoindex maybe; }\n")},
        {"NegativeBodyLimit", makeConfig(SERVER_DIRECTIVES + "client_max_body_size -1;\n")},
        {"InvalidBodyLimitSuffix", makeConfig(SERVER_DIRECTIVES + "client_max_body_size 1g;\n")},
    };

    INSTANTIATE_TEST_SUITE_P(
        Cases,
        ConfigLoaderInvalidTest,
        testing::ValuesIn(INVALID_CONFIGS),
        [](const testing::TestParamInfo<InvalidConfigCase>& info) { return std::string(info.param.name); }
    );

    TEST_F(ConfigLoaderTest, RejectsMissingFile) {
        const auto path = tempDir_.path() / "missing.conf";

        EXPECT_THROW(ConfigLoader::load(path.string()), ConfigError);
    }

    TEST_F(ConfigLoaderTest, RejectsDirectoryInsteadOfFile) {
        EXPECT_THROW(ConfigLoader::load(tempDir_.path().string()), ConfigError);
    }

    TEST_F(ConfigLoaderTest, AcceptsFileAtSizeLimit) {
        std::string source = makeConfig();

        source.resize(CONFIG_FILE_LIMIT, ' ');

        EXPECT_NO_THROW(loadText(source));
    }

    TEST_F(ConfigLoaderTest, RejectsFileOverSizeLimit) {
        std::string source = makeConfig();

        source.resize(CONFIG_FILE_LIMIT + 1, ' ');

        EXPECT_THROW(loadText(source), ConfigError);
    }

} // namespace
