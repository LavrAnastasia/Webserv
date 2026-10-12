#include <charconv>
#include <filesystem>
#include <fstream>
#include <functional>

#include <sys/wait.h>

#include <gtest/gtest.h>

#include "fs/FileDescriptor.hpp"
#include "includes/Files.hpp"
#include "includes/Requests.hpp"
#include "includes/TempDirectory.hpp"
#include "net/EventLoop.hpp"
#include "net/ServerSocket.hpp"
#include "net/TcpServer.hpp"

namespace {

    namespace fs = std::filesystem;
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    constexpr std::size_t kMebibyte = 1024u * 1024u;
    constexpr std::size_t kHeaderLimit = 64u * 1024u;
    constexpr std::size_t kResponseLimit = 256u * kMebibyte;
    constexpr std::size_t kCgiHugeSize = 200u * kMebibyte;
    constexpr std::size_t kCgiPostSize = 100u * kMebibyte;
    constexpr int kRepeatedRequests = 20;

    [[noreturn]] void systemError(const char* operation) {
        throw std::system_error(errno, std::generic_category(), operation);
    }

    void waitReady(int fd, short events, Clock::time_point deadline) {
        while (true) {
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();

            if (remaining <= 0) {
                throw std::runtime_error("Socket operation timed out");
            }

            pollfd descriptor{fd, events, 0};
            const int result = ::poll(&descriptor, 1, static_cast<int>(remaining));

            if (result < 0) {
                if (errno == EINTR) {
                    continue;
                }
                systemError("poll");
            }

            if (result == 0) {
                continue;
            }

            if (descriptor.revents & POLLNVAL) {
                throw std::runtime_error("Invalid socket descriptor");
            }

            if (descriptor.revents & (events | POLLERR | POLLHUP)) {
                return;
            }
        }
    }

    std::string lowercase(std::string value) {
        for (char& character : value) {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        return value;
    }

    struct ReceivedResponse {
        std::string statusLine;
        std::map<std::string, std::string> headers;
        std::string body;
        std::size_t bodySize = 0;
    };

    class TestClient {
    public:
        explicit TestClient(std::uint16_t port, int receiveBuffer = 0) : socket_(::socket(AF_INET, SOCK_STREAM, 0)) {
            if (socket_.get() < 0) {
                systemError("socket");
            }

            if (!socket_.setNonBlocking()) {
                systemError("fcntl");
            }

            if (receiveBuffer > 0 &&
                ::setsockopt(socket_.get(), SOL_SOCKET, SO_RCVBUF, &receiveBuffer, sizeof(receiveBuffer)) < 0) {
                systemError("setsockopt SO_RCVBUF");
            }

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons(port);

            if (::connect(socket_.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
                if (errno != EINPROGRESS && errno != EINTR) {
                    systemError("connect");
                }

                waitReady(socket_.get(), POLLOUT, Clock::now() + 5s);

                int error = 0;
                socklen_t size = sizeof(error);

                if (::getsockopt(socket_.get(), SOL_SOCKET, SO_ERROR, &error, &size) < 0) {
                    systemError("getsockopt SO_ERROR");
                }

                if (error != 0) {
                    throw std::system_error(error, std::generic_category(), "connect");
                }
            }
        }

        void sendAll(std::string_view bytes) {
            const auto deadline = Clock::now() + 5s;

            while (!bytes.empty()) {
                waitReady(socket_.get(), POLLOUT, deadline);

                const ssize_t sent = ::send(socket_.get(), bytes.data(), bytes.size(), MSG_NOSIGNAL);

                if (sent > 0) {
                    bytes.remove_prefix(static_cast<std::size_t>(sent));
                    continue;
                }

                if (sent < 0) {
                    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }
                    systemError("send");
                }

                throw std::runtime_error("send returned zero");
            }
        }

        ReceivedResponse
        readResponse(bool headersOnly = false, const std::function<void(std::string_view)>& consume = {}) {
            const auto deadline = Clock::now() + 30s;
            std::size_t headerEnd;

            while ((headerEnd = pending_.find("\r\n\r\n")) == std::string::npos) {
                if (pending_.size() > kHeaderLimit) {
                    throw std::runtime_error("Response headers are too large");
                }
                receiveMore(deadline);
            }

            if (headerEnd > kHeaderLimit) {
                throw std::runtime_error("Response headers are too large");
            }

            ReceivedResponse response;
            std::istringstream input(pending_.substr(0, headerEnd));

            std::getline(input, response.statusLine);
            if (!response.statusLine.empty() && response.statusLine.back() == '\r') {
                response.statusLine.pop_back();
            }

            for (std::string line; std::getline(input, line);) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                const auto separator = line.find(": ");
                if (separator == std::string::npos) {
                    throw std::runtime_error("Invalid response header: " + line);
                }

                const auto inserted =
                    response.headers.emplace(lowercase(line.substr(0, separator)), line.substr(separator + 2));

                if (!inserted.second) {
                    throw std::runtime_error("Duplicate response header");
                }
            }

            pending_.erase(0, headerEnd + 4);

            if (headersOnly || response.statusLine.starts_with("HTTP/1.1 204 ") ||
                response.statusLine.starts_with("HTTP/1.1 304 ")) {
                return response;
            }

            const auto encoding = response.headers.find("transfer-encoding");
            if (encoding != response.headers.end()) {
                if (encoding->second != "chunked" || response.headers.contains("content-length")) {
                    throw std::runtime_error("Invalid response framing");
                }
                while (true) {
                    std::size_t lineEnd;
                    while ((lineEnd = pending_.find("\r\n")) == std::string::npos) {
                        if (pending_.size() > kHeaderLimit) {
                            throw std::runtime_error("Chunk size line is too large");
                        }
                        receiveMore(deadline);
                    }
                    std::size_t size = 0;
                    const char* end = pending_.data() + lineEnd;
                    const auto parsed = std::from_chars(pending_.data(), end, size, 16);
                    if (parsed.ec != std::errc{} || parsed.ptr != end || size > kResponseLimit - response.bodySize) {
                        throw std::runtime_error("Invalid or excessive chunk size");
                    }
                    pending_.erase(0, lineEnd + 2);
                    consumeBody(size, deadline, response, consume);
                    while (pending_.size() < 2) {
                        receiveMore(deadline);
                    }
                    if (!pending_.starts_with("\r\n")) {
                        throw std::runtime_error("Missing chunk terminator");
                    }
                    pending_.erase(0, 2);
                    if (size == 0) {
                        return response;
                    }
                }
            }

            const auto lengthHeader = response.headers.find("content-length");
            if (lengthHeader == response.headers.end()) {
                throw std::runtime_error("Missing Content-Length");
            }

            const std::string& length = lengthHeader->second;
            std::size_t bodySize = 0;
            const auto [end, error] = std::from_chars(length.data(), length.data() + length.size(), bodySize);

            if (error != std::errc{} || end != length.data() + length.size() || bodySize > kResponseLimit) {
                throw std::runtime_error("Invalid or excessive Content-Length");
            }

            consumeBody(bodySize, deadline, response, consume);
            return response;
        }

        void waitForResponseStart() {
            if (pending_.empty()) {
                receiveMore(Clock::now() + 5s);
            }
        }

        void waitForBytes(std::string_view bytes) {
            const auto deadline = Clock::now() + 5s;
            while (pending_.find(bytes) == std::string::npos) {
                receiveMore(deadline);
            }
        }

        std::string readUntilEof() {
            std::string bytes = std::move(pending_);
            pending_.clear();
            const auto deadline = Clock::now() + 15s;

            while (true) {
                waitReady(socket_.get(), POLLIN, deadline);
                char buffer[16 * 1024];
                const ssize_t received = ::recv(socket_.get(), buffer, sizeof(buffer), 0);

                if (received == 0) {
                    return bytes;
                }

                if (received > 0) {
                    bytes.append(buffer, static_cast<std::size_t>(received));

                    if (bytes.size() > 64u * 1024u * 1024u) {
                        throw std::runtime_error("Response exceeds test limit");
                    }
                } else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                    systemError("recv");
                }
            }
        }

        void abortWithReset() {
            linger option{1, 0};

            if (::setsockopt(socket_.get(), SOL_SOCKET, SO_LINGER, &option, sizeof(option)) < 0) {
                systemError("setsockopt SO_LINGER");
            }

            socket_.close();
        }

        void closeWrite() {
            if (::shutdown(socket_.get(), SHUT_WR) < 0) {
                systemError("shutdown");
            }
        }

        void expectEof() {
            if (!pending_.empty()) {
                throw std::runtime_error("Unexpected bytes after response");
            }

            const auto deadline = Clock::now() + 5s;

            while (true) {
                waitReady(socket_.get(), POLLIN, deadline);

                char byte;
                const ssize_t received = ::recv(socket_.get(), &byte, 1, 0);

                if (received == 0) {
                    return;
                }

                if (received > 0) {
                    throw std::runtime_error("Unexpected bytes before EOF");
                }

                if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                    systemError("recv while waiting for EOF");
                }
            }
        }

    private:
        FileDescriptor socket_;
        std::string pending_;

        void consumeBody(
            std::size_t size,
            Clock::time_point deadline,
            ReceivedResponse& response,
            const std::function<void(std::string_view)>& consume
        ) {
            while (size > 0) {
                if (pending_.empty()) {
                    receiveMore(deadline);
                }
                const std::size_t count = std::min(size, pending_.size());
                if (consume) {
                    consume(std::string_view(pending_).substr(0, count));
                } else {
                    response.body.append(pending_, 0, count);
                }
                response.bodySize += count;
                pending_.erase(0, count);
                size -= count;
            }
        }

        void receiveMore(Clock::time_point deadline) {
            while (true) {
                waitReady(socket_.get(), POLLIN, deadline);

                char buffer[16 * 1024];
                const ssize_t received = ::recv(socket_.get(), buffer, sizeof(buffer), 0);

                if (received > 0) {
                    pending_.append(buffer, static_cast<std::size_t>(received));
                    return;
                }

                if (received == 0) {
                    throw std::runtime_error("Connection closed before the response was complete");
                }

                if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                    systemError("recv");
                }
            }
        }
    };

    class ServerTest : public ::testing::Test {
    protected:
        TempDirectory temp_{"webserv-server"};
        fs::path root_;
        pid_t child_ = -1;
        std::uint16_t port_ = 0;

        const std::string smallBody_ = "small response\n";
        const std::string otherBody_ = "another response\n";

        void SetUp() override {
            root_ = temp_.path();

            Files::write(root_ / "small.txt", smallBody_);
            Files::write(root_ / "other.txt", otherBody_);

            ServerConfig serverConfig;
            serverConfig.root = root_;
            serverConfig.clientMaxBodySize = kCgiPostSize;
            serverConfig.errorPages[HttpStatus::NotFound] = "error.html";

            serverConfig.listen = {{"127.0.0.1", 0}};

            LocationConfig location{};
            location.path = "/";
            location.allowedMethods = {HttpMethod::Get, HttpMethod::Head};

            LocationConfig cgi{};
            cgi.path = "/cgi";
            cgi.allowedMethods = {HttpMethod::Get, HttpMethod::Head, HttpMethod::Post};
            cgi.cgi.emplace(".sh", CgiConfig{".sh", fs::path("/bin/sh")});

            Files::write(root_ / "cgi/hello.sh", "printf 'Content-Type: text/plain\\r\\n\\r\\nhello from cgi'\n");
            Files::write(root_ / "cgi/bad.sh", "printf 'nonsense\\n\\n'\n");
            Files::write(root_ / "cgi/slow.sh", "sleep 3\nprintf 'Content-Type: text/plain\\r\\n\\r\\nlate'\n");
            Files::write(
                root_ / "cgi/large.sh",
                "printf 'Content-Type: application/octet-stream\\r\\n\\r\\n'\nhead -c 3000000 /dev/zero\n"
            );

            Files::write(root_ / "cgi/echo.sh", "printf 'Content-Type: text/plain\\r\\n\\r\\n'\ncat\n");
            Files::write(root_ / "cgi/marker.sh", "sleep 2\ntouch marker\n");
            Files::write(
                root_ / "cgi/huge.sh",
                "printf 'Content-Type: application/octet-stream\\r\\n\\r\\n'\nhead -c " + std::to_string(kCgiHugeSize) +
                    " /dev/zero\ntouch finished\n"
            );

            serverConfig.locations = {location, cgi};

            Configuration config;
            config.servers.push_back(serverConfig);

            auto server = std::make_unique<TcpServer>(config);
            const auto listeners = server->getListeningFds();

            ASSERT_EQ(listeners.size(), 1u);

            sockaddr_in address{};
            socklen_t addressSize = sizeof(address);

            ASSERT_EQ(::getsockname(listeners.front(), reinterpret_cast<sockaddr*>(&address), &addressSize), 0);

            port_ = ntohs(address.sin_port);
            ASSERT_NE(port_, 0);

            child_ = ::fork();
            ASSERT_GE(child_, 0);

            if (child_ == 0) {
                int exitCode = EXIT_SUCCESS;

                try {
                    EventLoop::setupSignals();

                    EventLoop loop(*server);
                    loop.initialize();
                    loop.run();
                } catch (const std::exception& error) {
                    std::fprintf(stderr, "Test server: %s\n", error.what());
                    exitCode = EXIT_FAILURE;
                }

                server.reset();
                ::_exit(exitCode);
            }

            server.reset();

            expectFreshConnectionWorks();
        }

        void TearDown() override {
            if (child_ > 0) {
                int status = 0;

                const auto reap = [&](int options) {
                    pid_t result;
                    do {
                        result = ::waitpid(child_, &status, options);
                    } while (result < 0 && errno == EINTR);
                    return result;
                };

                pid_t result = reap(WNOHANG);

                if (result == child_) {
                    ADD_FAILURE() << "Server exited before test cleanup";
                } else if (result == 0) {
                    EXPECT_EQ(::kill(child_, SIGTERM), 0);

                    const auto deadline = Clock::now() + 2s;

                    do {
                        result = reap(WNOHANG);
                        if (result != 0) {
                            break;
                        }
                        ::poll(nullptr, 0, 10);
                    } while (Clock::now() < deadline);

                    if (result == 0) {
                        ADD_FAILURE() << "Server did not stop after SIGTERM";
                        ::kill(child_, SIGKILL);
                        result = reap(0);
                    }
                }

                EXPECT_EQ(result, child_);

                if (result == child_) {
                    EXPECT_TRUE(WIFEXITED(status)) << "Process wait status: " << status;

                    if (WIFEXITED(status)) {
                        EXPECT_EQ(WEXITSTATUS(status), EXIT_SUCCESS);
                    }
                }

                child_ = -1;
            }
        }

        std::string createLargeFile() const {
            std::string body(8u * 1024u * 1024u + 137u, '\0');

            for (std::size_t i = 0; i < body.size(); ++i) {
                body[i] = static_cast<char>((i * 37 + i / 251) % 256);
            }

            Files::write(root_ / "large.bin", body);
            return body;
        }

#ifdef __linux__
        std::uintmax_t readPeakMemory() const {
            std::ifstream status("/proc/" + std::to_string(child_) + "/status");
            for (std::string line; std::getline(status, line);) {
                if (line.starts_with("VmHWM:")) {
                    std::istringstream value(line.substr(6));
                    std::uintmax_t kib = 0;
                    if (value >> kib) {
                        return kib * 1024;
                    }
                }
            }
            throw std::runtime_error("Cannot read server peak memory");
        }
#endif

        void expectOK(TestClient& client, const std::string& expected) const {
            const auto response = client.readResponse();

            EXPECT_EQ(response.statusLine, "HTTP/1.1 200 OK");
            ASSERT_EQ(response.body.size(), expected.size());

            EXPECT_TRUE(response.body == expected) << "Response body differs from the expected file contents";
        }

        void expectFreshConnectionWorks() const {
            TestClient client(port_);
            client.sendAll(Requests::rawGet("/small.txt", "close"));
            expectOK(client, smallBody_);
            client.expectEof();
        }
    };

    TEST_F(ServerTest, HeadOmitsFileBodyAndNextGetIncludesIt) {
        const std::string expected = createLargeFile();
        TestClient client(port_);
        client.sendAll("HEAD /large.bin HTTP/1.1\r\nHost: localhost\r\n\r\n");

        const auto response = client.readResponse(true);

        EXPECT_EQ(response.statusLine, "HTTP/1.1 200 OK");
        EXPECT_EQ(response.headers.at("content-length"), std::to_string(expected.size()));
        EXPECT_TRUE(response.body.empty());

        client.sendAll(Requests::rawGet("/large.bin", "close"));
        expectOK(client, expected);
        client.expectEof();
    }

    TEST_F(ServerTest, HeadOmitsCustomErrorBodyAndNextGetIncludesIt) {
        const std::string expected(128u * 1024u + 17u, 'e');
        Files::write(root_ / "error.html", expected);

        TestClient client(port_);
        client.sendAll("HEAD /missing.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");

        const auto head = client.readResponse(true);

        EXPECT_EQ(head.statusLine, "HTTP/1.1 404 Not Found");
        EXPECT_EQ(head.headers.at("content-length"), std::to_string(expected.size()));
        EXPECT_TRUE(head.body.empty());

        client.sendAll(Requests::rawGet("/missing.txt", "close"));
        const auto get = client.readResponse();

        EXPECT_EQ(get.statusLine, "HTTP/1.1 404 Not Found");
        EXPECT_EQ(get.body, expected);
        client.expectEof();
    }

    TEST_F(ServerTest, ServesSeveralClientsWithoutMixingTheirResponses) {
        constexpr int clientCount = 8;
        std::vector<std::unique_ptr<TestClient>> clients;

        for (int i = 0; i < clientCount; ++i) {
            const std::string name = "client-" + std::to_string(i) + ".txt";
            Files::write(root_ / name, "response for client " + std::to_string(i));

            clients.push_back(std::make_unique<TestClient>(port_));

            const std::string request = Requests::rawGet("/" + name, "close");
            clients.back()->sendAll(request.substr(0, request.size() - 2));
        }

        for (auto& client : clients) {
            client->sendAll("\r\n");
        }

        for (int i = clientCount - 1; i >= 0; --i) {
            SCOPED_TRACE(i);

            expectOK(*clients[i], "response for client " + std::to_string(i));
            clients[i]->expectEof();
        }
    }

    TEST_F(ServerTest, IncompleteRequestDoesNotBlockOtherClients) {
        TestClient slow(port_);
        slow.sendAll("GET /small.txt HTTP/1.1\r\nHost:");

        expectFreshConnectionWorks();

        slow.sendAll(" localhost\r\nConnection: close\r\n\r\n");
        expectOK(slow, smallBody_);
        slow.expectEof();
    }

    TEST_F(ServerTest, AnswersCgiAndReusesTheConnection) {
        TestClient client(port_);
        client.sendAll(Requests::rawGet("/cgi/hello.sh"));
        const auto response = client.readResponse();
        EXPECT_EQ(response.statusLine, "HTTP/1.1 200 OK");
        EXPECT_EQ(response.body, "hello from cgi");
        EXPECT_EQ(response.headers.at("transfer-encoding"), "chunked");
        EXPECT_FALSE(response.headers.contains("content-length"));

        client.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(client, smallBody_);
        client.expectEof();
    }

    TEST_F(ServerTest, HeadOnCgiOmitsTheBody) {
        TestClient client(port_);
        client.sendAll("HEAD /cgi/hello.sh HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");

        const auto response = client.readResponse(true);

        EXPECT_EQ(response.statusLine, "HTTP/1.1 200 OK");
        EXPECT_FALSE(response.headers.contains("transfer-encoding"));
        EXPECT_FALSE(response.headers.contains("content-length"));
        client.expectEof();
    }

    TEST_F(ServerTest, PreservesCgiContentLengthAndReusesConnection) {
        Files::write(
            root_ / "cgi/length.sh",
            "printf 'Content-Type: text/plain\\r\\nContent-Length: 8\\r\\n\\r\\ncomplete'\nexit 1\n"
        );
        TestClient client(port_);
        client.sendAll(Requests::rawGet("/cgi/length.sh"));
        const auto response = client.readResponse();
        EXPECT_EQ(response.statusLine, "HTTP/1.1 200 OK");
        EXPECT_EQ(response.headers.at("content-length"), "8");
        EXPECT_FALSE(response.headers.contains("transfer-encoding"));
        EXPECT_EQ(response.body, "complete");
        client.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(client, smallBody_);
        client.expectEof();
    }

    TEST_F(ServerTest, DrainsHeadAndBodylessCgiOutputBeforeNextResponse) {
        for (const int status : {200, 204, 304}) {
            SCOPED_TRACE(status);
            Files::write(
                root_ / "cgi/bodyless.sh",
                "printf 'Status: " + std::to_string(status) +
                    "\\r\\nContent-Type: text/plain\\r\\nContent-Length: 123\\r\\n\\r\\n'\n"
                    "head -c 3000000 /dev/zero\n"
            );
            TestClient client(port_);
            const std::string method = status == 200 ? "HEAD" : "GET";
            client.sendAll(method + " /cgi/bodyless.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
            const auto response = client.readResponse(status == 200);
            EXPECT_TRUE(response.statusLine.starts_with("HTTP/1.1 " + std::to_string(status) + " "));
            EXPECT_FALSE(response.headers.contains("transfer-encoding"));
            EXPECT_TRUE(response.body.empty());
            client.sendAll(Requests::rawGet("/small.txt", "close"));
            expectOK(client, smallBody_);
            client.expectEof();
        }
    }

    TEST_F(ServerTest, AnswersBadGatewayForInvalidCgiOutputAndKeepsServing) {
        TestClient client(port_);
        client.sendAll(Requests::rawGet("/cgi/bad.sh", "close"));

        EXPECT_EQ(client.readResponse().statusLine, "HTTP/1.1 502 Bad Gateway");
        client.expectEof();
        expectFreshConnectionWorks();
    }

    TEST_F(ServerTest, SendsLargeCgiOutputWhole) {
        TestClient client(port_);
        client.sendAll(Requests::rawGet("/cgi/large.sh", "close"));

        expectOK(client, std::string(3000000, '\0'));
        client.expectEof();
    }

    TEST_F(ServerTest, ClientLeavingDuringCgiDoesNotAffectOthers) {
        TestClient leaving(port_);
        leaving.sendAll(Requests::rawGet("/cgi/slow.sh"));
        ::poll(nullptr, 0, 200);
        leaving.abortWithReset();

        expectFreshConnectionWorks();
    }

    TEST_F(ServerTest, PassesTheRequestBodyToCgi) {
        TestClient client(port_);
        client.sendAll("POST /cgi/echo.sh HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\nping");
        expectOK(client, "ping");

        client.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(client, smallBody_);
        client.expectEof();
    }

    TEST_F(ServerTest, Streams200MiBToSlowClientWithBoundedMemory) {
#ifdef __linux__
        constexpr std::size_t kMemoryGrowthLimit = 16u * kMebibyte;
        const auto before = readPeakMemory();
#endif
        TestClient client(port_, 64 * 1024);
        client.sendAll(Requests::rawGet("/cgi/huge.sh"));
        client.waitForResponseStart();
        ::poll(nullptr, 0, 200);
        EXPECT_FALSE(fs::exists(root_ / "cgi/finished"));
        expectFreshConnectionWorks();
#ifdef __linux__
        EXPECT_LT(readPeakMemory(), before + kMemoryGrowthLimit);
#endif
        bool valid = true;
        std::size_t received = 0;
        std::size_t nextPause = kMebibyte;
        const auto response = client.readResponse(false, [&](std::string_view bytes) {
            valid = valid && bytes.find_first_not_of('\0') == std::string_view::npos;
            received += bytes.size();
            if (received >= nextPause) {
                nextPause += kMebibyte;
                ::poll(nullptr, 0, 2);
#ifdef __linux__
                EXPECT_LT(readPeakMemory(), before + kMemoryGrowthLimit);
#endif
            }
        });
        EXPECT_EQ(response.statusLine, "HTTP/1.1 200 OK");
        EXPECT_EQ(response.headers.at("transfer-encoding"), "chunked");
        EXPECT_EQ(response.bodySize, kCgiHugeSize);
        EXPECT_TRUE(valid);
        EXPECT_TRUE(fs::exists(root_ / "cgi/finished"));
        client.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(client, smallBody_);
        client.expectEof();
    }

    TEST_F(ServerTest, EchoesTwenty100MiBPostsThroughCgi) {
        TestClient client(port_);
        constexpr std::size_t blockSize = 64u * 1024u;
        const std::string block(blockSize, 'e');
        for (int request = 0; request < kRepeatedRequests; ++request) {
            SCOPED_TRACE(request);
            client.sendAll(
                "POST /cgi/echo.sh HTTP/1.1\r\nHost: localhost\r\nContent-Length: " + std::to_string(kCgiPostSize) +
                "\r\n\r\n"
            );
            for (std::size_t sent = 0; sent < kCgiPostSize; sent += block.size()) {
                client.sendAll(block);
            }
            bool valid = true;
            const auto response = client.readResponse(false, [&](std::string_view bytes) {
                valid = valid && bytes.find_first_not_of('e') == std::string_view::npos;
            });
            ASSERT_EQ(response.statusLine, "HTTP/1.1 200 OK");
            ASSERT_EQ(response.bodySize, kCgiPostSize);
            ASSERT_TRUE(valid);
            EXPECT_EQ(response.headers.at("transfer-encoding"), "chunked");
        }
        client.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(client, smallBody_);
        client.expectEof();
    }

    TEST_F(ServerTest, CompletesTwentySmallCgiRequestsWithin200MillisecondsEach) {
        Files::write(root_ / "cgi/nonzero.sh", "printf 'Content-Type: text/plain\\r\\n\\r\\ncomplete'\nexit 1\n");
        TestClient client(port_);
        for (int request = 0; request < kRepeatedRequests; ++request) {
            SCOPED_TRACE(request);
            const auto start = Clock::now();
            client.sendAll(Requests::rawGet("/cgi/nonzero.sh"));
            expectOK(client, "complete");
            EXPECT_LT(Clock::now() - start, 200ms);
        }
        client.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(client, smallBody_);
        client.expectEof();
    }

    TEST_F(ServerTest, FinishesCgiResponseAtPipeEofBeforeProcessExit) {
        Files::write(
            root_ / "cgi/eof.sh", "printf 'Content-Type: text/plain\\r\\n\\r\\ncomplete'\nexec 1>&-\nsleep 3\nexit 1\n"
        );
        TestClient client(port_);
        const auto start = Clock::now();
        client.sendAll(Requests::rawGet("/cgi/eof.sh"));
        expectOK(client, "complete");
        EXPECT_LT(Clock::now() - start, 200ms);
        client.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(client, smallBody_);
        client.expectEof();
    }

    TEST_F(ServerTest, ClosesCgiResponseWhenScriptExitsDuringContentLengthBody) {
        Files::write(
            root_ / "cgi/truncated.sh",
            "printf 'Content-Type: text/plain\\r\\nContent-Length: 10\\r\\n\\r\\npartial'\n"
            "while [ ! -e release ]; do sleep 0.01; done\nexit 1\n"
        );
        TestClient client(port_);
        client.sendAll(Requests::rawGet("/cgi/truncated.sh"));
        client.waitForBytes("partial");
        Files::write(root_ / "cgi/release", "");
        const auto response = client.readUntilEof();
        EXPECT_TRUE(response.starts_with("HTTP/1.1 200 OK\r\n"));
        EXPECT_TRUE(response.ends_with("\r\n\r\npartial"));
        EXPECT_EQ(response.find("502 Bad Gateway"), std::string::npos);
        expectFreshConnectionWorks();
    }

    TEST_F(ServerTest, StopsCgiWhenTheClientCloses) {
        {
            TestClient leaving(port_);
            leaving.sendAll(Requests::rawGet("/cgi/marker.sh"));
            ::poll(nullptr, 0, 300);
        }

        ::poll(nullptr, 0, 3000);

        EXPECT_FALSE(fs::exists(root_ / "cgi/marker"));
        expectFreshConnectionWorks();
    }

    TEST_F(ServerTest, AnswersARequestPipelinedBehindCgi) {
        TestClient client(port_);
        client.sendAll(Requests::rawGet("/cgi/hello.sh") + Requests::rawGet("/small.txt", "close"));

        expectOK(client, "hello from cgi");
        expectOK(client, smallBody_);
        client.expectEof();
    }

    class ServerKeepAliveTest : public ServerTest, public ::testing::WithParamInterface<bool> {};

    TEST_P(ServerKeepAliveTest, ReusesConnectionForSeveralRequests) {
        TestClient client(port_);

        const std::string connection = GetParam() ? "keep-alive" : "";

        for (int i = 0; i < 6; ++i) {
            SCOPED_TRACE(i);

            const bool useOther = i % 2 != 0;
            client.sendAll(Requests::rawGet(useOther ? "/other.txt" : "/small.txt", connection));

            expectOK(client, useOther ? otherBody_ : smallBody_);
        }

        client.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(client, smallBody_);
        client.expectEof();
    }

    INSTANTIATE_TEST_SUITE_P(
        Persistence,
        ServerKeepAliveTest,
        ::testing::Values(false, true),
        [](const ::testing::TestParamInfo<bool>& info) {
            return info.param ? std::string("ExplicitKeepAlive") : std::string("DefaultHttp11");
        }
    );

    TEST_F(ServerTest, DisconnectDuringRequestDoesNotAffectOtherClients) {
        TestClient survivor(port_);

        for (const bool reset : {false, true}) {
            SCOPED_TRACE(reset);

            {
                TestClient dropped(port_);
                dropped.sendAll("GET /small.txt HTTP/1.1\r\nHost:");

                if (reset) {
                    dropped.abortWithReset();
                }
            }

            survivor.sendAll(Requests::rawGet("/other.txt"));
            expectOK(survivor, otherBody_);

            expectFreshConnectionWorks();
        }

        survivor.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(survivor, smallBody_);
        survivor.expectEof();
    }

    TEST_F(ServerTest, ResetDuringLargeResponseDoesNotStopServer) {
        createLargeFile();

        TestClient survivor(port_);
        TestClient dropped(port_, 64 * 1024);

        dropped.sendAll(Requests::rawGet("/large.bin"));

        dropped.waitForResponseStart();
        dropped.abortWithReset();

        survivor.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(survivor, smallBody_);
        survivor.expectEof();

        expectFreshConnectionWorks();
    }

    TEST_F(ServerTest, AnswersRequestAfterClientClosesWriteSide) {
        for (int i = 0; i < 20; ++i) {
            SCOPED_TRACE(i);
            TestClient client(port_);

            client.sendAll(Requests::rawGet("/other.txt"));
            client.closeWrite();

            expectOK(client, otherBody_);
            client.expectEof();
        }
    }

    TEST_F(ServerTest, SendsLargeResponseAfterClientClosesWriteSide) {
        const std::string body = createLargeFile();
        TestClient client(port_);

        client.sendAll(Requests::rawGet("/large.bin"));
        client.closeWrite();

        expectOK(client, body);
        client.expectEof();
    }

    TEST_F(ServerTest, SendsCompleteLargeBinaryResponseAndReusesConnection) {
        const std::string body = createLargeFile();
        TestClient client(port_);

        client.sendAll(Requests::rawGet("/large.bin"));
        expectOK(client, body);

        client.sendAll(Requests::rawGet("/other.txt", "close"));
        expectOK(client, otherBody_);
        client.expectEof();
    }

    TEST_F(ServerTest, SlowReaderDoesNotBlockOtherClients) {
        const std::string body = createLargeFile();

        TestClient slow(port_, 64 * 1024);
        slow.sendAll(Requests::rawGet("/large.bin"));
        slow.waitForResponseStart();

        expectFreshConnectionWorks();

        expectOK(slow, body);

        slow.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(slow, smallBody_);
        slow.expectEof();
    }

    TEST_F(ServerTest, ServesEmptyAndChunkBoundaryFilesWithContentLength) {
        TestClient client(port_);

        for (const std::size_t size : {0u, 65535u, 65536u, 65537u}) {
            SCOPED_TRACE(size);
            const std::string expected(size, '\0');
            Files::write(root_ / "boundary.bin", expected);
            client.sendAll(Requests::rawGet("/boundary.bin"));
            const auto response = client.readResponse();

            EXPECT_EQ(response.statusLine, "HTTP/1.1 200 OK");
            EXPECT_EQ(response.headers.at("content-length"), std::to_string(size));
            EXPECT_EQ(response.headers.count("transfer-encoding"), 0u);
            EXPECT_EQ(response.body, expected);
        }

        client.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(client, smallBody_);
        client.expectEof();
    }

    TEST_F(ServerTest, ClosesTruncatedResponseAndKeepsServingOtherClients) {
        const std::string body = createLargeFile();
        TestClient client(port_, 64 * 1024);
        client.sendAll(Requests::rawGet("/large.bin"));
        client.waitForResponseStart();

        fs::resize_file(root_ / "large.bin", 0);
        const std::string response = client.readUntilEof();
        const auto separator = response.find("\r\n\r\n");
        ASSERT_NE(separator, std::string::npos);
        EXPECT_TRUE(response.starts_with("HTTP/1.1 200 OK\r\n"));
        EXPECT_NE(response.find("Content-Length: " + std::to_string(body.size()) + "\r\n"), std::string::npos);

        const std::string received = response.substr(separator + 4);
        ASSERT_LT(received.size(), body.size());
        EXPECT_TRUE(body.starts_with(received));
        expectFreshConnectionWorks();
    }

    TEST_F(ServerTest, FileGrowthDoesNotCorruptTheNextResponse) {
        const std::string body = createLargeFile();
        TestClient client(port_, 64 * 1024);
        client.sendAll(Requests::rawGet("/large.bin"));
        client.waitForResponseStart();

        {
            std::ofstream file(root_ / "large.bin", std::ios::binary | std::ios::app);
            file << "bytes beyond Content-Length";
            ASSERT_TRUE(file);
        }

        expectOK(client, body);
        client.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(client, smallBody_);
        client.expectEof();
    }

    TEST_F(ServerTest, StreamsCustomErrorPageAndReusesConnection) {
        const std::string expected(3 * 65536 + 17, 'e');
        Files::write(root_ / "error.html", expected);
        TestClient client(port_);
        client.sendAll(Requests::rawGet("/missing"));
        const auto response = client.readResponse();

        EXPECT_EQ(response.statusLine, "HTTP/1.1 404 Not Found");
        EXPECT_EQ(response.headers.at("content-length"), std::to_string(expected.size()));
        EXPECT_EQ(response.body, expected);

        client.sendAll(Requests::rawGet("/small.txt", "close"));
        expectOK(client, smallBody_);
        client.expectEof();
    }

    TEST_F(ServerTest, SlowLargeTransfersKeepMemoryBounded) {
#ifdef __linux__
        const auto peakMemory = [&]() {
            std::ifstream status("/proc/" + std::to_string(child_) + "/status");

            for (std::string line; std::getline(status, line);) {
                if (line.starts_with("VmHWM:")) {
                    std::istringstream value(line.substr(6));
                    std::uintmax_t kib = 0;

                    if (value >> kib) {
                        return kib * 1024;
                    }
                }
            }

            throw std::runtime_error("Cannot read server peak memory");
        };

        Files::write(root_ / "huge.bin", "");
        fs::resize_file(root_ / "huge.bin", 256u * 1024u * 1024u);
        const auto before = peakMemory();
        std::vector<std::unique_ptr<TestClient>> clients;

        for (int i = 0; i < 3; ++i) {
            clients.push_back(std::make_unique<TestClient>(port_, 64 * 1024));
            clients.back()->sendAll(Requests::rawGet("/huge.bin"));
            clients.back()->waitForResponseStart();
            expectFreshConnectionWorks();

            ASSERT_LT(peakMemory(), before + 16u * 1024u * 1024u);
        }

        for (auto& client : clients) {
            client->abortWithReset();
        }

        expectFreshConnectionWorks();
#else
        GTEST_SKIP() << "Peak memory check uses Linux /proc";
#endif
    }

    TEST(ServerSocketTest, ResolvesNumericHostsAndFormatsAcceptedAddress) {
        for (const std::string host : {"127.0.0.1", "0.0.0.0"}) {
            SCOPED_TRACE(host);
            ServerSocket server(host, 0);
            server.setNonBlocking();
            sockaddr_in address{};
            socklen_t size = sizeof(address);
            ASSERT_EQ(::getsockname(server.getFd(), reinterpret_cast<sockaddr*>(&address), &size), 0);
            EXPECT_EQ(ntohl(address.sin_addr.s_addr), host == "0.0.0.0" ? INADDR_ANY : INADDR_LOOPBACK);

            TestClient client(ntohs(address.sin_port));
            waitReady(server.getFd(), POLLIN, Clock::now() + 5s);
            std::string ip;
            std::uint16_t port = 0;
            FileDescriptor accepted(server.acceptConnection(ip, port));

            ASSERT_TRUE(accepted.isOpen());
            EXPECT_EQ(ip, "127.0.0.1");
            EXPECT_NE(port, 0);
        }
    }

    TEST(ServerSocketTest, RejectsInvalidNumericHost) {
        EXPECT_THROW(ServerSocket("999.0.0.1", 0), std::invalid_argument);
    }

} // namespace
