#pragma once

#include "ConnectionRegistry.hpp"
#include "Poller.hpp"
#include "net/TcpServer.hpp"
#include <vector>

class EventLoop {
private:
    Poller poller_;
    ConnectionRegistry connectionRegistry_;
    TcpServer& tcpServer_;

    bool isRunning_;
    int clientTimeoutSeconds_;
    std::vector<int> listeningFds_;

    void handleNewConnection(int listenFd);
    void handleClientActivity(int clientFd, uint32_t events);
    void cleanupTimedOutConnections();

public:
    EventLoop(TcpServer& server) : tcpServer_(server), isRunning_(false), clientTimeoutSeconds_(60) {}
    ~EventLoop() = default;

    void initialize();
    void run();
    void stop();
};
