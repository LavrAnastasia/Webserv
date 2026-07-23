#pragma once

#include <poll.h>
#include <vector>

class Poller {
private:
    std::vector<pollfd> pollFds_;

public:
    void addSocket(int fd);
    void modifySocket(int fd, short newEvents);
    void removeSocket(int fd);
    std::vector<pollfd> waitForEvents();
};
