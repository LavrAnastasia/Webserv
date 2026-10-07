#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include <gtest/gtest.h>

#include "http/HttpSerializer.hpp"
#include "includes/Files.hpp"
#include "includes/Responses.hpp"
#include "includes/TempDirectory.hpp"

namespace {
    namespace fs = std::filesystem;

    class ResponseBodyTest : public ::testing::Test {
    protected:
        TempDirectory temp_{"webserv-body"};
    };

    TEST_F(ResponseBodyTest, StreamsStringsAndFilesWithinTheRequestedLimit) {
        for (const std::size_t size : {0u, 1u, 65535u, 65536u, 65537u, 196625u}) {
            SCOPED_TRACE(size);
            std::string expected(size, '\0');

            for (std::size_t i = 0; i < size; ++i) {
                expected[i] = static_cast<char>(i % 256);
            }

            const auto path = temp_.path() / "payload.bin";
            Files::write(path, expected);

            for (const bool file : {false, true}) {
                SCOPED_TRACE(file);
                auto opened = ResponseBody::open(path);
                ASSERT_TRUE(std::holds_alternative<ResponseBody>(opened));
                ResponseBody body = file ? std::move(std::get<ResponseBody>(opened)) : ResponseBody(expected);
                EXPECT_EQ(body.size(), size);
                std::string received;

                while (!body.done()) {
                    auto chunk = body.next(65536);
                    ASSERT_TRUE(chunk);
                    ASSERT_FALSE(chunk->empty());
                    ASSERT_LE(chunk->size(), 65536u);
                    received.append(*chunk);
                }

                EXPECT_EQ(received, expected);
                EXPECT_EQ(body.size(), size);
                ASSERT_TRUE(body.next(65536));
                EXPECT_TRUE(body.next(65536)->empty());
            }
        }
    }

    TEST_F(ResponseBodyTest, RejectsMissingFilesAndDirectories) {
        auto missing = ResponseBody::open(temp_.path() / "missing");
        ASSERT_TRUE(std::holds_alternative<std::error_code>(missing));
        EXPECT_EQ(std::get<std::error_code>(missing), std::errc::no_such_file_or_directory);

        auto directory = ResponseBody::open(temp_.path());
        ASSERT_TRUE(std::holds_alternative<std::error_code>(directory));
        EXPECT_EQ(std::get<std::error_code>(directory), std::errc::permission_denied);
    }

    TEST_F(ResponseBodyTest, SerializationDoesNotReadTheFileBody) {
        const auto path = temp_.path() / "payload.bin";
        Files::write(path, "initial contents");
        auto opened = ResponseBody::open(path);
        ASSERT_TRUE(std::holds_alternative<ResponseBody>(opened));

        auto output = HttpSerializer::serialize(
            HttpResponse{.status = HttpStatus::OK, .body = std::move(std::get<ResponseBody>(opened))}, {}
        );

        EXPECT_NE(output.headers.find("Content-Length: 16\r\n"), std::string::npos);
        EXPECT_TRUE(output.headers.ends_with("\r\n\r\n"));
        ASSERT_TRUE(output.body);

        fs::resize_file(path, 0);
        EXPECT_FALSE(output.body->next(65536));
        EXPECT_FALSE(output.body->done());
    }

    TEST_F(ResponseBodyTest, ShortReadIsDataAndPrematureEofIsAnError) {
        const auto path = temp_.path() / "payload.bin";
        Files::write(path, "abcdef");
        auto opened = ResponseBody::open(path);
        ASSERT_TRUE(std::holds_alternative<ResponseBody>(opened));
        auto& body = std::get<ResponseBody>(opened);

        fs::resize_file(path, 3);
        auto first = body.next(65536);
        ASSERT_TRUE(first);
        EXPECT_EQ(*first, "abc");
        EXPECT_FALSE(body.done());
        EXPECT_FALSE(body.next(65536));
    }

    TEST_F(ResponseBodyTest, StopsAtTheOriginalSizeWhenFileGrows) {
        const auto path = temp_.path() / "payload.bin";
        Files::write(path, "original");
        auto opened = ResponseBody::open(path);
        ASSERT_TRUE(std::holds_alternative<ResponseBody>(opened));

        {
            std::ofstream file(path, std::ios::binary | std::ios::app);
            file << "extra bytes";
            ASSERT_TRUE(file);
        }

        auto& body = std::get<ResponseBody>(opened);
        EXPECT_EQ(body.size(), 8u);
        EXPECT_EQ(Responses::read(body), "original");
    }

    TEST_F(ResponseBodyTest, KeepsTheOpenedFileWhenThePathIsReplaced) {
        const auto path = temp_.path() / "payload.bin";
        Files::write(path, "original");
        auto opened = ResponseBody::open(path);
        ASSERT_TRUE(std::holds_alternative<ResponseBody>(opened));

        fs::rename(path, temp_.path() / "old.bin");
        Files::write(path, "replacement");

        EXPECT_EQ(Responses::read(std::get<ResponseBody>(opened)), "original");
    }

    TEST_F(ResponseBodyTest, SuppressesFileBodiesForHeadersOnlyAndBodylessStatuses) {
        const auto path = temp_.path() / "payload.bin";
        Files::write(path, "payload");

        for (const auto status : {HttpStatus::OK, HttpStatus::NoContent, HttpStatus::NotModified}) {
            for (const bool headersOnly : {false, true}) {
                SCOPED_TRACE(static_cast<int>(status));
                SCOPED_TRACE(headersOnly);
                auto opened = ResponseBody::open(path);
                ASSERT_TRUE(std::holds_alternative<ResponseBody>(opened));
                auto output = HttpSerializer::serialize(
                    HttpResponse{.status = status, .body = std::move(std::get<ResponseBody>(opened))},
                    {.headersOnly = headersOnly}
                );

                EXPECT_EQ(output.body.has_value(), status == HttpStatus::OK && !headersOnly);
                EXPECT_EQ(output.headers.find("Content-Length: 7\r\n") != std::string::npos, status == HttpStatus::OK);
            }
        }
    }

    static_assert(!std::is_copy_constructible_v<ResponseBody>);
    static_assert(std::is_nothrow_move_constructible_v<ResponseBody>);
} // namespace
