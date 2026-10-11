#include "cgi/CgiRegistry.hpp"

#include <utility>

void CgiRegistry::add(int clientFd, CgiProcess process) {
    entries_.insert_or_assign(clientFd, Entry{std::move(process), std::chrono::steady_clock::now(), std::nullopt});
}

void CgiRegistry::remove(int clientFd) {
    entries_.erase(clientFd);
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
    if (pipeFd < 0) {
        return std::nullopt;
    }

    for (const auto& [clientFd, entry] : entries_) {
        if (entry.process.inputFd() == pipeFd || entry.process.outputFd() == pipeFd) {
            return clientFd;
        }
    }

    return std::nullopt;
}

std::vector<int> CgiRegistry::expired(std::chrono::seconds timeout, std::chrono::steady_clock::time_point now) const {
    std::vector<int> result;

    for (const auto& [clientFd, entry] : entries_) {
        if (!entry.paused && now - entry.activity >= timeout) {
            result.push_back(clientFd);
        }
    }

    return result;
}

void CgiRegistry::touch(int clientFd) {
    auto it = entries_.find(clientFd);
    if (it != entries_.end()) {
        const auto now = std::chrono::steady_clock::now();
        it->second.activity = now;
        if (it->second.paused) {
            it->second.paused = now;
        }
    }
}

void CgiRegistry::pause(int clientFd, bool paused) {
    auto it = entries_.find(clientFd);
    if (it == entries_.end()) {
        return;
    }
    auto& entry = it->second;
    const auto now = std::chrono::steady_clock::now();
    if (paused && !entry.paused) {
        entry.paused = now;
    } else if (!paused && entry.paused) {
        entry.activity += now - *entry.paused;
        entry.paused.reset();
    }
}

std::vector<int> CgiRegistry::clients() const {
    std::vector<int> result;
    result.reserve(entries_.size());
    for (const auto& [clientFd, entry] : entries_) {
        (void)entry;
        result.push_back(clientFd);
    }
    return result;
}
