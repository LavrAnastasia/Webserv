#pragma once

#include "net/Connection.hpp"

#include <string>
#include <unordered_map>
#include <vector>

struct ServerConfig;

class ConnectionRegistry {
private:
    /*
    Unordered map (a highly optimized hash table) is used to store connection objects
    because of performance: when OS tells Poller that a given fd has incoming data,
    fetching the matching Connection object takes only 1 operation.

    activeConnections_ maps OS file descriptor to Connection object
*/
    std::unordered_map<int, Connection> activeConnections_;

public:
    /*
    The default keyword guarantees the compiler will generate (highly optimized)
    default constructors, which will not be skipped even if a parametrized constructor
    is added later.
*/
    ConnectionRegistry() = default;
    ~ConnectionRegistry() = default;

    // called by TCP server when new client connects
    void addConnection(int fd, const std::string& ip, const ServerConfig* config);

    // called by event loop to clean up disconnected client
    void removeConnection(int fd);

    /*
    called by event loop when poller indicates the fd has data
    returns a pointer, because a reference would break and crash the program
    in case of abrupt client disconnection
*/
    Connection* getConnection(int fd) const;

    /*
    deletes timed out connections from activeConnections_ and returns a vector
    of the pruned fds for the poller to stop tracking
*/
    std::vector<int> pruneConnections(int timeoutSeconds);
};
