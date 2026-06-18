#include "ConnectionRegistry.hpp"

/*
    -called by TCP server when new client connects
    -creates a key/value pair in activeConnections_,
    where the fd is the key and a Connection object is the value
*/
void ConnectionRegistry::addConnection(int fd, const std::string& ip, int port) {
    activeConnections_.try_emplace(fd, fd, ip, port);
}

/*
    -called by event loop to clean up disconnected client
    -erase() automatically calls the Connection destructor and
    frees all memory. If the fd doesn't exist, erase safely does nothing
*/
void ConnectionRegistry::removeConnection(int fd) {
    activeConnections_.erase(fd);
}

/*
    called by event loop when poller indicates the fd has data
    returns a pointer, because a reference would break and crash the program
    in case of abrupt client disconnection
    -find() returns an iterator pointing to the key/value pair, or end() if not found
    -end() is a theoretical invalid memory space just past the last item in the map
    -it->first is the KEY of the key/value pair
    -it->second is the VALUE of the pair, in this case a Connection object
    -const makes every variable accessed by the function temporarily const,
    but const_cast removes this lock and makes the returned Connection pointer mutable
*/
Connection* ConnectionRegistry::getConnection(int fd) const {
    auto it = activeConnections_.find(fd);
    if (it != activeConnections_.end()) {
        return const_cast<Connection*>(&(it->second));
    }
    return nullptr;
}

/*
deletes timed out connections from activeConnections_ and returns a vector
of the pruned fds for the poller to stop tracking
*/
std::vector<int> ConnectionRegistry::pruneConnections(int timeoutSeconds) {
}

/*
    Connection::Connection(int fd, const std::string& ip, int port)
    : clientIp_(ip), clientPort_(port), currentState_(State::RECEIVING) {
    fd_ = fd;
    lastActivity_ = time(nullptr); //set start of timeout timer
    }

    try_emplace(fd, fd, ip, port);
    1. fd = map key
    2. - 4. = constructor arguments passed to Connection constructor
    "Calculate hash for File Descriptor *** and find the correct memory bucket,
    then pass the rest of the arguments to the Connection constructor and build the object
    directly in that bucket without copying -> EFFICIENT!"
*/
