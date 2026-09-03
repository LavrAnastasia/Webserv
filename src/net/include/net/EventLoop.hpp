#pragma once

#include "net/ConnectionRegistry.hpp"
#include "net/Poller.hpp"
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

    //address of active loop for OS to direct signals to
    static EventLoop* instance_;
    //called when signal received from OS
    static void handleSignal(int sig);

    void handleNewConnection(int listenFd);
    void handleClientActivity(int clientFd, uint32_t events);
    void cleanupTimedOutConnections();

public:
    EventLoop(TcpServer& server) : tcpServer_(server), isRunning_(false), clientTimeoutSeconds_(60) {}

    //links this loop instance with OS signal system
    static void registerSignalHandler(EventLoop* instance);

    void initialize();
    void run();
    void stop();
};
