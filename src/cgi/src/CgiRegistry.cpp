#include "cgi/CgiRegistry.hpp"

#include <iterator>
#include <utility>

void CgiRegistry::add(int clientFd, CgiProcess process) {
    remove(clientFd);
    activity_.push_back(clientFd);

    const int input = process.inputFd();
    const int output = process.outputFd();

    try {
        entries_.emplace(
            clientFd, Entry{std::move(process), std::chrono::steady_clock::now(), std::prev(activity_.end()), false}
        );
    } catch (...) {
        activity_.pop_back();
        throw;
    }

    if (input >= 0) {
        pipes_.insert_or_assign(input, clientFd);
    }

    pipes_.insert_or_assign(output, clientFd);
}

void CgiRegistry::remove(int clientFd) {
    auto it = entries_.find(clientFd);

    if (it == entries_.end()) {
        return;
    }

    release(it->second.process.inputFd());
    release(it->second.process.outputFd());
    activity_.erase(it->second.activity);
    entries_.erase(it);
}

void CgiRegistry::release(int pipeFd) {
    pipes_.erase(pipeFd);
}

void CgiRegistry::touch(int clientFd) {
    auto it = entries_.find(clientFd);

    if (it == entries_.end()) {
        return;
    }

    it->second.updated = std::chrono::steady_clock::now();
    activity_.splice(activity_.end(), activity_, it->second.activity);
}

std::optional<int> CgiRegistry::pause(int clientFd) {
    auto it = entries_.find(clientFd);

    if (it == entries_.end() || it->second.paused) {
        return std::nullopt;
    }

    it->second.paused = true;
    return it->second.process.outputFd();
}

std::optional<int> CgiRegistry::resume(int clientFd) {
    auto it = entries_.find(clientFd);

    if (it == entries_.end() || !it->second.paused) {
        return std::nullopt;
    }

    it->second.paused = false;
    return it->second.process.outputFd();
}

std::size_t CgiRegistry::size() const {
    return entries_.size();
}

CgiProcess* CgiRegistry::find(int clientFd) {
    auto it = entries_.find(clientFd);

    if (it == entries_.end()) {
        return nullptr;
    }

    return &it->second.process;
}

std::optional<int> CgiRegistry::client(int pipeFd) const {
    const auto it = pipes_.find(pipeFd);

    if (it == pipes_.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::vector<int> CgiRegistry::expired(std::chrono::seconds timeout, std::chrono::steady_clock::time_point now) const {
    std::vector<int> result;

    for (int clientFd : activity_) {
        if (now - entries_.at(clientFd).updated < timeout) {
            break;
        }

        result.push_back(clientFd);
    }

    return result;
}
