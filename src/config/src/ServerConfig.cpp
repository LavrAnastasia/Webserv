#include "config/ServerConfig.hpp"

#include <algorithm>

std::size_t ServerConfig::maxBodySize() const {
    std::size_t size = clientMaxBodySize;

    for (const LocationConfig& location : locations) {
        size = std::max(size, location.clientMaxBodySize.value_or(clientMaxBodySize));
    }

    return size;
}
