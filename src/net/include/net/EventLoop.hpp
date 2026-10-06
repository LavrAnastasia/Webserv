#pragma once

#include "cgi/CgiRegistry.hpp"
#include "http/CgiRequest.hpp"
#include "net/ConnectionRegistry.hpp"
#include "net/Poller.hpp"
#include "net/TcpServer.hpp"

#include <chrono>
#include <csignal>
#include <vector>

class EventLoop {
private:
    static volatile std::sig_atomic_t stopRequested_;
    static void handleSignal(int sig);

    Poller poller_;
    ConnectionRegistry connectionRegistry_;
    CgiRegistry cgiRegistry_;
    TcpServer& tcpServer_;

    int clientTimeoutSeconds_;
    std::vector<int> listeningFds_;
    bool acceptEventsEnabled_ = true;
    std::chrono::steady_clock::time_point acceptAgainAt_;

    void handleNewConnection(int listenFd);
    void handleClientActivity(int clientFd, uint32_t events);
    void handleCgiActivity(int clientFd, int pipeFd);
    void launchCgi(Connection& connection, const CgiRequest& request);
    void closeCgi(int clientFd);
    void closeConnection(int fd);
    void cleanupTimedOutConnections();
    void cleanupTimedOutCgi();
    void updateAcceptEvents();

public:
    static void setupSignals();

    EventLoop(TcpServer& server) : tcpServer_(server), clientTimeoutSeconds_(60) {}

    void initialize();
    void run();
};
