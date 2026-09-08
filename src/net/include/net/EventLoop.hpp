#pragma once

#include "net/ConnectionRegistry.hpp"
#include "net/Poller.hpp"
#include "net/TcpServer.hpp"

#include <csignal>
#include <vector>

class EventLoop {
private:
    static volatile std::sig_atomic_t stopRequested_;
    static void handleSignal(int sig);

    Poller poller_;
    ConnectionRegistry connectionRegistry_;
    TcpServer& tcpServer_;

    int clientTimeoutSeconds_;
    std::vector<int> listeningFds_;

    void handleNewConnection(int listenFd);
    void handleClientActivity(int clientFd, uint32_t events);
    void cleanupTimedOutConnections();

public:
    static void setupSignals();

    EventLoop(TcpServer& server) : tcpServer_(server), clientTimeoutSeconds_(60) {}

    void initialize();
    void run();
    void stop();
};
