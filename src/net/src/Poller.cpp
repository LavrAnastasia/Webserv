#include "net/Poller.hpp"
#include <cerrno> // for 'errno' variable and EINTR constant
#include <cstring> // for std::strerror()
#include <stdexcept> // for std::runtime_error

void Poller::addSocket(int fd) {
    pollfd newEvent;
    newEvent.fd = fd; // the socket to monitor
    newEvent.events = POLLIN; // POLLIN = watch for inbound data
    newEvent.revents = 0; // result: filled in by OS
    pollFds_.push_back(newEvent);
}


void Poller::removeSocket(int fd) {
    std::erase_if(pollFds_, [fd](const pollfd& p) { return p.fd == fd; });
}

std::vector<int> Poller::waitForEvents() {
    int activeCount = poll(pollFds_.data(), pollFds_.size(), -1); //how many sockets active?
    if (activeCount < 0) {
        if (errno == EINTR) {
            return {}; // benign OS interruption -no need to crash server
        }
        throw std::runtime_error("NetError: poll() failed: " + std::string(std::strerror(errno)));
    }
    std::vector<int> activeSockets;
    for (const pollfd& p : pollFds_) {
        if (p.revents & POLLIN) {
            activeSockets.push_back(p.fd);
        } // check p.revents value with bitwise 'and' operation
    }
    return activeSockets;
}

/*
    how pollfd.revents flags are checked: if (p.revents & POLLIN) etc.
    common pollfd.revents flags:    POLLIN (data ready to read)
                                    POLLOUT (data ready to write)
                                    POLLHUP (client closed connection)
                                    POLLERR (abrupt failure ex. crashed router)

    erase_if syntax:    pollFds_ = the list to look at
                        [fd] = capture clause (what to look for)
                        (const pollfd& p) = parameter (p = temporary variable name)
                        {
                            return p.fd == fd:
                        } = rule: if p.fd matces [fd], delete (and exit function)
                        NOTE: whole list is always checked, and return keyword
                        is internal to erase_if (item is deleted if return value == true)


    poll() function signature: int poll(struct pollfd *fds, nfds_t nfds, int timeout);
        struct pollfd *fds = pointer to array of pollfd structs (poll is written in C so no vectors)
        nfds_t nfds = number of structs in the array
        int timeout = how long OS should wait for events before returning to program
                    -if > 0: wait for specified number of ms.
                    -if 0: check sockets immediately without pausing program
                    -if < 0: infinite block, wake up only when socket receives data
        NOTE: pollFds_.data() returns C-style memory address of first item in vector,
        and is necessary since poll() doesn't understand vectors
*/
