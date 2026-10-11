#pragma once

#include <chrono>
#include <optional>
#include <unordered_map>
#include <vector>

#include "cgi/CgiProcess.hpp"

class CgiRegistry {
private:
    struct Entry {
        CgiProcess process;
        std::chrono::steady_clock::time_point activity;
        std::optional<std::chrono::steady_clock::time_point> paused;
    };

    std::unordered_map<int, Entry> entries_;

public:
    void add(int clientFd, CgiProcess process);
    void remove(int clientFd);
    void touch(int clientFd);
    void pause(int clientFd, bool paused);
    std::vector<int> clients() const;

    std::size_t size() const;
    CgiProcess* find(int clientFd);
    std::optional<int> client(int pipeFd) const;
    std::vector<int> expired(std::chrono::seconds timeout, std::chrono::steady_clock::time_point now) const;
};
