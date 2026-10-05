#include <charconv>
#include <filesystem>

#include <gtest/gtest.h>

#include "fs/FileDescriptor.hpp"
#include "includes/Files.hpp"
#include "includes/Requests.hpp"
#include "includes/TempDirectory.hpp"
#include "net/EventLoop.hpp"
#include "net/TcpServer.hpp"

namespace {

    namespace fs = std::filesystem;
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

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

        ReceivedResponse readResponse() {
            const auto deadline = Clock::now() + 15s;
            std::size_t headerEnd;

            while ((headerEnd = pending_.find("\r\n\r\n")) == std::string::npos) {
                if (pending_.size() > 64 * 1024) {
                    throw std::runtime_error("Response headers are too large");
                }
                receiveMore(deadline);
            }

            if (headerEnd > 64 * 1024) {
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

            const auto lengthHeader = response.headers.find("content-length");
            if (lengthHeader == response.headers.end()) {
                throw std::runtime_error("Missing Content-Length");
            }

            const std::string& length = lengthHeader->second;
            std::size_t bodySize = 0;

            const auto [end, error] = std::from_chars(length.data(), length.data() + length.size(), bodySize);

            if (error != std::errc{} || end != length.data() + length.size() || bodySize > 64u * 1024u * 1024u) {
                throw std::runtime_error("Invalid or excessive Content-Length");
            }

            const std::size_t bodyStart = headerEnd + 4;
            const std::size_t messageSize = bodyStart + bodySize;

            while (pending_.size() < messageSize) {
                receiveMore(deadline);
            }

            response.body = pending_.substr(bodyStart, bodySize);
            pending_.erase(0, messageSize);
            return response;
        }

        void waitForResponseStart() {
            if (pending_.empty()) {
                receiveMore(Clock::now() + 5s);
            }
        }

        void abortWithReset() {
            linger option{1, 0};

            if (::setsockopt(socket_.get(), SOL_SOCKET, SO_LINGER, &option, sizeof(option)) < 0) {
                systemError("setsockopt SO_LINGER");
            }

            socket_.close();
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

            serverConfig.listen = {{"127.0.0.1", 0}};

            LocationConfig location{};
            location.path = "/";
            location.allowedMethods = {HttpMethod::Get};
            serverConfig.locations = {location};

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

} // namespace
