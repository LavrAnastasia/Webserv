#include "cgi/CgiRegistry.hpp"

#include <utility>

void CgiRegistry::add(int clientFd, CgiProcess process) {
    entries_.insert_or_assign(clientFd, Entry{std::move(process), std::chrono::steady_clock::now()});
}

void CgiRegistry::remove(int clientFd) {
    entries_.erase(clientFd);
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
        if (now - entry.launched >= timeout) {
            result.push_back(clientFd);
        }
    }

    return result;
}
