#include <algorithm>
#include <cerrno>
#include <csignal>
#include <filesystem>
#include <memory>
#include <string>
#include <sys/socket.h>
#include <utility>
#include <variant>

#include <gtest/gtest.h>

#include "config/ServerConfig.hpp"
#include "fs/FileDescriptor.hpp"
#include "includes/Files.hpp"
#include "includes/TempDirectory.hpp"
#include "net/Connection.hpp"

namespace {
    namespace fs = std::filesystem;

    class ConnectionTest : public ::testing::Test {
    protected:
        TempDirectory temp_{"webserv-connection"};
        ServerConfig config_;
        FileDescriptor peer_;
        std::unique_ptr<Connection> connection_;
        using SignalHandler = void (*)(int);
        SignalHandler previousSigpipe_ = SIG_DFL;

        void SetUp() override {
            previousSigpipe_ = std::signal(SIGPIPE, SIG_IGN);
            int sockets[2];
            ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
            peer_ = FileDescriptor(sockets[1]);
            connection_ = std::make_unique<Connection>(sockets[0], "127.0.0.1", 8080, config_);

            const int bufferSize = 128 * 1024;
            ASSERT_EQ(::setsockopt(connection_->getFd(), SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize)), 0);
        }

        void TearDown() override { std::signal(SIGPIPE, previousSigpipe_); }

        ResponseBody fileBody(const std::string& contents) {
            const auto path = temp_.path() / "payload.bin";
            Files::write(path, contents);
            auto opened = ResponseBody::open(path);
            return std::move(std::get<ResponseBody>(opened));
        }

        std::string receiveAvailable() {
            std::string received;
            char buffer[8192];

            while (true) {
                const ssize_t count = ::recv(peer_.get(), buffer, sizeof(buffer), MSG_DONTWAIT);

                if (count > 0) {
                    received.append(buffer, static_cast<std::size_t>(count));
                } else if (count < 0 && errno == EINTR) {
                    continue;
                } else {
                    EXPECT_TRUE(count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
                    return received;
                }
            }
        }

        void expectResponse(const std::string& received, const std::string& body, std::size_t length) {
            const auto separator = received.find("\r\n\r\n");
            ASSERT_NE(separator, std::string::npos);
            EXPECT_NE(received.find("Content-Length: " + std::to_string(length) + "\r\n"), std::string::npos);
            EXPECT_TRUE(received.substr(separator + 4) == body);
        }
    };

    TEST_F(ConnectionTest, SendsHeadersAndEntireMemoryBodyTogether) {
        for (const std::size_t size : {0u, 1u, 65537u}) {
            SCOPED_TRACE(size);
            const std::string body(size, '\0');
            connection_->setResponse(HttpResponse{.status = HttpStatus::NotFound, .body = ResponseBody(body)});

            ASSERT_EQ(connection_->sendResponse(), Connection::SendResult::Ok);
            EXPECT_TRUE(connection_->isSendComplete());
            expectResponse(receiveAvailable(), body, size);
        }
    }

    TEST_F(ConnectionTest, SendsHeadersAndFirstFileChunkTogether) {
        for (const std::size_t size : {0u, 1u, 65536u, 65537u}) {
            SCOPED_TRACE(size);
            const std::string body(size, 'f');
            connection_->setResponse(HttpResponse{.status = HttpStatus::OK, .body = fileBody(body)});

            ASSERT_EQ(connection_->sendResponse(), Connection::SendResult::Ok);
            EXPECT_EQ(connection_->isSendComplete(), size <= 65536);
            expectResponse(receiveAvailable(), body.substr(0, 65536), size);

            if (!connection_->isSendComplete()) {
                ASSERT_EQ(connection_->sendResponse(), Connection::SendResult::Ok);
                EXPECT_TRUE(connection_->isSendComplete());
                EXPECT_EQ(receiveAvailable(), body.substr(65536));
            }
        }
    }

    TEST_F(ConnectionTest, HeadDoesNotReadOrSendTheFileBody) {
        auto body = fileBody("payload");
        fs::resize_file(temp_.path() / "payload.bin", 0);
        connection_->setHeadersOnly(true);
        connection_->setResponse(HttpResponse{.status = HttpStatus::OK, .body = std::move(body)});

        ASSERT_EQ(connection_->sendResponse(), Connection::SendResult::Ok);
        EXPECT_TRUE(connection_->isSendComplete());
        expectResponse(receiveAvailable(), "", 7);
    }

    TEST_F(ConnectionTest, ReportsTruncationBeforeSendingHeadersAndCanReplaceFailedResponse) {
        auto body = fileBody("payload");
        fs::resize_file(temp_.path() / "payload.bin", 0);
        connection_->setResponse(HttpResponse{.status = HttpStatus::OK, .body = std::move(body)});

        EXPECT_EQ(connection_->sendResponse(), Connection::SendResult::BodyError);
        EXPECT_FALSE(connection_->isSendComplete());
        EXPECT_TRUE(receiveAvailable().empty());

        connection_->setResponse(HttpResponse{.status = HttpStatus::OK, .body = ResponseBody("replacement")});
        ASSERT_EQ(connection_->sendResponse(), Connection::SendResult::Ok);
        EXPECT_TRUE(connection_->isSendComplete());
        expectResponse(receiveAvailable(), "replacement", 11);
    }

    TEST_F(ConnectionTest, ReportsTruncationAfterTheFirstChunkAsBodyError) {
        const std::string body(2 * 65536, 'f');
        connection_->setResponse(HttpResponse{.status = HttpStatus::OK, .body = fileBody(body)});
        ASSERT_EQ(connection_->sendResponse(), Connection::SendResult::Ok);
        expectResponse(receiveAvailable(), body.substr(0, 65536), body.size());
        fs::resize_file(temp_.path() / "payload.bin", 0);

        EXPECT_EQ(connection_->sendResponse(), Connection::SendResult::BodyError);
        EXPECT_FALSE(connection_->isSendComplete());
        EXPECT_TRUE(receiveAvailable().empty());
    }

    TEST_F(ConnectionTest, PreservesBytesAcrossPartialWritesForMemoryAndFileBodies) {
        const int bufferSize = 4096;
        ASSERT_EQ(::setsockopt(connection_->getFd(), SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize)), 0);
        std::string expected(3 * 65536 + 17, '\0');

        for (std::size_t i = 0; i < expected.size(); ++i) {
            expected[i] = static_cast<char>(i % 256);
        }

        for (const bool file : {false, true}) {
            SCOPED_TRACE(file);
            connection_->setResponse(
                HttpResponse{.status = HttpStatus::OK, .body = file ? fileBody(expected) : ResponseBody(expected)}
            );
            std::string received;
            int sends = 0;

            while (!connection_->isSendComplete()) {
                ASSERT_LT(sends++, 1024);
                ASSERT_EQ(connection_->sendResponse(), Connection::SendResult::Ok);
                received.append(receiveAvailable());
            }

            EXPECT_GT(sends, 1);
            expectResponse(received, expected, expected.size());
        }
    }

    TEST_F(ConnectionTest, ReportsSocketFailureSeparatelyFromBodyFailure) {
        connection_->setResponse(HttpResponse{.status = HttpStatus::OK, .body = ResponseBody("payload")});
        ASSERT_EQ(::shutdown(connection_->getFd(), SHUT_WR), 0);

        EXPECT_EQ(connection_->sendResponse(), Connection::SendResult::SocketError);
    }
} // namespace
