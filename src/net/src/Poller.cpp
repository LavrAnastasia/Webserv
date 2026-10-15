#include "net/Poller.hpp"
#include <cerrno> // errno and EINTR
#include <system_error>

void Poller::addSocket(int fd) {
    pollfd newEvent;
    newEvent.fd = fd; // the socket to watch
    newEvent.events = POLLIN; // wake up when there is data to read
    newEvent.revents = 0; // filled in by poll()
    pollFds_.push_back(newEvent);
}

/*
    called by EventLoop to switch a socket between receive and send modes
    POLLIN  - wake up when the client has sent bytes
    POLLOUT - wake up when there is room to send queued response bytes
*/
void Poller::modifySocket(int fd, short newEvents) {
    for (pollfd& p : pollFds_) {
        if (p.fd == fd) {
            p.events = newEvents;
            break;
        }
    }
}

void Poller::removeSocket(int fd) {
    std::erase_if(pollFds_, [fd](const pollfd& p) { return p.fd == fd; });
}

std::vector<pollfd> Poller::waitForEvents() {
    int activeCount = poll(pollFds_.data(), pollFds_.size(), 1000); // waits at most 1 s, so timeouts are checked
    if (activeCount < 0) {
        if (errno == EINTR) {
            return {}; // interrupted by a signal, not an error
        }
        throw std::system_error(errno, std::generic_category(), "poll()");
    }
    std::vector<pollfd> activeSockets;
    for (const pollfd& p : pollFds_) {
        // revents != 0: something happened on this socket
        if (p.revents != 0) {
            activeSockets.push_back(p);
        }
    }
    return activeSockets;
}

/*
    revents flags, checked as (p.revents & FLAG):
        POLLIN  - data ready to read
        POLLOUT - room to write
        POLLHUP - peer hung up (on macOS it can mean the peer only stopped sending)
        POLLERR - socket error

    poll(fds, count, timeout): timeout in ms; > 0 waits up to that long, 0 returns at once, < 0 waits forever.
    pollFds_.data() passes the vector as the C array poll() expects.
*/
