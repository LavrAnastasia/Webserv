#pragma once

#include <memory> // needed for std::unique_ptr
#include <vector>

#include "config/ServerConfig.hpp"

class ServerSocket;

class SocketManager {
private:
    /*
    Changed raw pointers to smart (unique) pointers. Unique pointers owned by
    SocketManager are deleted automatically if SocketManager is destroyed.
    */
    std::vector<std::unique_ptr<ServerSocket>> servers_;

public:
    SocketManager() = default;
    ~SocketManager() = default; //unique_ptr handles delete automatically

    void createServers(const std::vector<ServerConfig>& configs);

    /*
    Rule: use smart pointers to define who OWNS data,
    and raw pointers or references to define who USES it.
    In this case: "here's a socket you can use, but don't need to
    worry about deleting"
    */
    std::vector<ServerSocket*> getServers() const;
};
