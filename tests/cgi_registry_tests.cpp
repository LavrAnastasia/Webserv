#include <chrono>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "cgi/CgiProcess.hpp"
#include "cgi/CgiRegistry.hpp"

namespace {
    using Clock = std::chrono::steady_clock;
    constexpr std::chrono::seconds kTimeout{60};
    constexpr int kFirstClient = 100;
    constexpr int kSecondClient = 101;

    std::optional<CgiProcess> launch() {
        return CgiProcess::launch(CgiRequest{"/bin/sh", "/dev/null", {}, "input"});
    }

    TEST(CgiRegistryTest, ExpiresFromLastOutputInActivityOrder) {
        CgiRegistry registry;
        auto first = launch();
        auto second = launch();
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        registry.add(kFirstClient, std::move(*first));
        registry.add(kSecondClient, std::move(*second));

        const auto boundary = Clock::now();
        registry.touch(kFirstClient);

        EXPECT_EQ(registry.expired(kTimeout, boundary + kTimeout), std::vector<int>{kSecondClient});
        EXPECT_EQ(registry.expired(kTimeout, Clock::now() + kTimeout), (std::vector<int>{kSecondClient, kFirstClient}));
    }

    TEST(CgiRegistryTest, PausesAndResumesWithoutExtendingTheOutputTimeout) {
        CgiRegistry registry;
        auto process = launch();
        ASSERT_TRUE(process);
        const int output = process->outputFd();
        registry.add(kFirstClient, std::move(*process));
        const auto boundary = Clock::now();

        EXPECT_FALSE(registry.resume(kFirstClient));
        EXPECT_EQ(registry.pause(kFirstClient), output);
        EXPECT_FALSE(registry.pause(kFirstClient));
        EXPECT_EQ(registry.resume(kFirstClient), output);
        EXPECT_FALSE(registry.resume(kFirstClient));
        EXPECT_EQ(registry.expired(kTimeout, boundary + kTimeout), std::vector<int>{kFirstClient});
    }

    TEST(CgiRegistryTest, ReleasesClosedInputAndRemovesRemainingRoutesAndTimeouts) {
        CgiRegistry registry;
        auto process = launch();
        ASSERT_TRUE(process);
        const int input = process->inputFd();
        const int output = process->outputFd();
        registry.add(kFirstClient, std::move(*process));
        EXPECT_EQ(registry.client(input), kFirstClient);
        EXPECT_EQ(registry.client(output), kFirstClient);

        registry.find(kFirstClient)->closeInput();
        registry.release(input);
        EXPECT_FALSE(registry.client(input));
        EXPECT_EQ(registry.client(output), kFirstClient);

        registry.remove(kFirstClient);
        EXPECT_FALSE(registry.client(output));
        EXPECT_EQ(registry.find(kFirstClient), nullptr);
        EXPECT_TRUE(registry.expired(kTimeout, Clock::now() + kTimeout).empty());
    }

    TEST(CgiRegistryTest, ReplacesProcessWithoutKeepingItsPipeRoutes) {
        CgiRegistry registry;
        auto first = launch();
        auto replacement = launch();
        ASSERT_TRUE(first);
        ASSERT_TRUE(replacement);
        const int oldInput = first->inputFd();
        const int oldOutput = first->outputFd();
        const int newInput = replacement->inputFd();
        const int newOutput = replacement->outputFd();

        registry.add(kFirstClient, std::move(*first));
        registry.add(kFirstClient, std::move(*replacement));

        EXPECT_FALSE(registry.client(oldInput));
        EXPECT_FALSE(registry.client(oldOutput));
        EXPECT_EQ(registry.client(newInput), kFirstClient);
        EXPECT_EQ(registry.client(newOutput), kFirstClient);
        EXPECT_EQ(registry.size(), 1u);
        EXPECT_EQ(registry.expired(kTimeout, Clock::now() + kTimeout), std::vector<int>{kFirstClient});
    }
} // namespace
