#include "net/EventLoop.hpp"

/*
    void handleNewConnection(int listenFd);
    void handleClientActivity(int clientFd, uint32_t events);
    void performGarbageCollection();

public:
    EventLoop(TcpServer& server)
        : tcpServer_(server), isRunning_(false), clientTimeoutSeconds_(60) {}
    ~EventLoop() = default;

    void initialize();
    void run();
    void stop();
*/

void EventLoop::initialize() {
    listeningFds_ = tcpServer_.getListeningFds();
    for (int fd : listeningFds_) {
        poller_.addSocket(fd);
    }
}

void EventLoop::run() {
    isRunning_ = true;

    while (isRunning_) {
        auto activeSockets = poller_.waitForEvents();
    }
}
