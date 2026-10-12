#pragma once

#include <chrono>
#include <list>
#include <optional>
#include <unordered_map>
#include <vector>

#include "cgi/CgiProcess.hpp"

class CgiRegistry {
private:
    struct Entry {
        CgiProcess process;
        std::chrono::steady_clock::time_point updated;
        std::list<int>::iterator activity;
        bool paused;
    };

    std::unordered_map<int, Entry> entries_;
    std::unordered_map<int, int> pipes_;
    std::list<int> activity_;

public:
    void add(int clientFd, CgiProcess process);
    void remove(int clientFd);
    void release(int pipeFd);
    void touch(int clientFd);
    std::optional<int> pause(int clientFd);
    std::optional<int> resume(int clientFd);

    std::size_t size() const;
    CgiProcess* find(int clientFd);
    std::optional<int> client(int pipeFd) const;
    std::vector<int> expired(std::chrono::seconds timeout, std::chrono::steady_clock::time_point now) const;
};
