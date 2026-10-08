#pragma once

#include <cstdint>
#include <string>

struct ConnectionInfo {
    std::string remoteAddr;
    std::uint16_t serverPort = 0;
};
