#include <cerrno>
#include <csignal>
#include <memory>
#include <string>
#include <sys/socket.h>

#include <gtest/gtest.h>

#include "fs/FileDescriptor.hpp"
#include "net/Connection.hpp"

namespace {
    class ConnectionTest : public ::testing::Test {
    protected:
        FileDescriptor peer_;
        std::unique_ptr<Connection> connection_;
        using SignalHandler = void (*)(int);
        SignalHandler previousSigpipe_ = SIG_DFL;

        void SetUp() override {
            previousSigpipe_ = std::signal(SIGPIPE, SIG_IGN);
            int sockets[2];
            ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
            peer_ = FileDescriptor(sockets[1]);
            connection_ = std::make_unique<Connection>(sockets[0], ConnectionInfo{"127.0.0.1", 8080});
        }

        void TearDown() override { std::signal(SIGPIPE, previousSigpipe_); }

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

        static std::string pattern(std::size_t size) {
            std::string bytes(size, '\0');

            for (std::size_t i = 0; i < size; ++i) {
                bytes[i] = static_cast<char>(i % 256);
            }

            return bytes;
        }
    };

    TEST_F(ConnectionTest, ReceivesBytesAndReportsTheClientLeaving) {
        ASSERT_EQ(::send(peer_.get(), "hello", 5, 0), 5);

        const auto bytes = connection_->receive();

        ASSERT_TRUE(bytes.has_value());
        EXPECT_EQ(*bytes, "hello");

        peer_.close();
        EXPECT_FALSE(connection_->receive().has_value());
    }

    TEST_F(ConnectionTest, SendsQueuedBytes) {
        connection_->queue("response");

        EXPECT_EQ(connection_->pending(), 8u);
        ASSERT_TRUE(connection_->send());
        EXPECT_EQ(connection_->pending(), 0u);
        EXPECT_EQ(receiveAvailable(), "response");
    }

    TEST_F(ConnectionTest, SendWithNothingQueuedSucceeds) {
        EXPECT_TRUE(connection_->send());
        EXPECT_EQ(connection_->pending(), 0u);
    }

    TEST_F(ConnectionTest, KeepsBytesAcrossPartialSendsAndLaterQueues) {
        const int bufferSize = 4096;
        ASSERT_EQ(::setsockopt(connection_->getFd(), SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize)), 0);
        const std::string first = pattern(3 * 65536 + 17);
        const std::string second = "tail";

        connection_->queue(first);
        ASSERT_TRUE(connection_->send());
        ASSERT_GT(connection_->pending(), 0u);
        connection_->queue(second);

        std::string received = receiveAvailable();
        int sends = 1;

        while (connection_->pending() != 0) {
            ASSERT_LT(sends++, 4096);
            ASSERT_TRUE(connection_->send());
            received.append(receiveAvailable());
        }

        EXPECT_EQ(received, first + second);
    }

    TEST_F(ConnectionTest, ReportsSocketFailure) {
        connection_->queue("payload");
        ASSERT_EQ(::shutdown(connection_->getFd(), SHUT_WR), 0);

        EXPECT_FALSE(connection_->send());
    }
} // namespace
