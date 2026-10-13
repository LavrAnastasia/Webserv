#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "cgi/CgiProcess.hpp"
#include "cgi/CgiRegistry.hpp"

namespace {
    using Clock = std::chrono::steady_clock;
    constexpr std::chrono::seconds kTimeout{60};
    constexpr std::chrono::milliseconds kClockTick{1};
    constexpr int kFirstClient = 100;
    constexpr int kSecondClient = 101;

    std::optional<CgiProcess> launch() {
        return CgiProcess::launch(CgiRequest{"/bin/sh", "/dev/null", {}, "input"});
    }

    TEST(CgiRegistryTest, ExpiresFromLastOutput) {
        CgiRegistry registry;
        auto first = launch();
        auto second = launch();
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        registry.add(kFirstClient, std::move(*first));
        registry.add(kSecondClient, std::move(*second));

        const auto boundary = Clock::now();
        std::this_thread::sleep_for(kClockTick);
        registry.touch(kFirstClient);

        EXPECT_EQ(registry.expired(kTimeout, boundary + kTimeout), std::vector<int>{kSecondClient});
        auto expired = registry.expired(kTimeout, Clock::now() + kTimeout);
        std::ranges::sort(expired);
        EXPECT_EQ(expired, (std::vector<int>{kFirstClient, kSecondClient}));
    }

    TEST(CgiRegistryTest, SkipsPausedProcessesWhenOthersExpire) {
        CgiRegistry registry;
        auto first = launch();
        auto second = launch();
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        registry.add(kFirstClient, std::move(*first));
        registry.add(kSecondClient, std::move(*second));

        ASSERT_TRUE(registry.pause(kFirstClient));
        EXPECT_EQ(registry.expired(kTimeout, Clock::now() + kTimeout), std::vector<int>{kSecondClient});
    }

    TEST(CgiRegistryTest, RestartsTheOutputTimeoutOnResume) {
        CgiRegistry registry;
        auto process = launch();
        ASSERT_TRUE(process);
        const int output = process->outputFd();
        registry.add(kFirstClient, std::move(*process));
        const auto boundary = Clock::now();

        EXPECT_FALSE(registry.resume(kFirstClient));
        EXPECT_EQ(registry.pause(kFirstClient), output);
        EXPECT_FALSE(registry.pause(kFirstClient));
        EXPECT_TRUE(registry.expired(kTimeout, boundary + kTimeout).empty());
        std::this_thread::sleep_for(kClockTick);
        EXPECT_EQ(registry.resume(kFirstClient), output);
        EXPECT_FALSE(registry.resume(kFirstClient));
        EXPECT_TRUE(registry.expired(kTimeout, boundary + kTimeout).empty());
        EXPECT_EQ(registry.expired(kTimeout, Clock::now() + kTimeout), std::vector<int>{kFirstClient});
    }

    TEST(CgiRegistryTest, IgnoresClosedInputAndRemovesRemainingDescriptorsAndTimeouts) {
        CgiRegistry registry;
        auto process = launch();
        ASSERT_TRUE(process);
        const int input = process->inputFd();
        const int output = process->outputFd();
        registry.add(kFirstClient, std::move(*process));
        EXPECT_EQ(registry.client(input), kFirstClient);
        EXPECT_EQ(registry.client(output), kFirstClient);

        registry.find(kFirstClient)->closeInput();
        EXPECT_FALSE(registry.client(input));
        EXPECT_FALSE(registry.client(-1));
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
