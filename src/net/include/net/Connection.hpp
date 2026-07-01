#pragma once

#include "net/Socket.hpp"

#include <ctime>
#include <string>

/*
    The connection class:
    This class is the bridge between the OS and high-level HTTP logic.
    It receives the chopped-up TCP transmission sent by the client, combines
    the pieces and holds the message string for the server to retrieve.

    Tasks:
        1.  buffer data: saves the pieces of a HTTP request in a receive buffer
            until the whole request has arrived.
        2.  manage id: stores socket fd and IP address of the client
        3.  track state:
                RECEIVING - HTTP request not yet fully received from client
                PARSING - request received and sent to HTTP parser
                SENDING - sending response to client
                CLOSED - sequence complete, or fatal error occurred

    Inputs:
        1.  From client (via OS): raw bytes to receive with C system call recv()
        2.  From server logic: HTTP response string like "HTTP/1.1 200 OK\r\n\r\n<html>..."

    Outputs:
        1.  To server:  complete, validated HTTP request string (which server passes
                        onto the HTTP parser)
        2.  To client (via OS): HTTP response from server, converted into raw bytes
            and sent with C system call send()

    Interacts with :
        1.  Poller (indirectly): when poller tells server that a socket has data,
                server tells corresponding connection to call receiveRequest()
        2.  HTTP parser (indirectly): when the connection has received a complete request
                string, server pulls it out and passes it onto the HTTP parser
        3. Event loop: event loop tracks state of each connection, and when it sees
                CLOSED, it closes the corresponding fd and deletes the connection
*/

//forward declaration sufficient for pointer
struct ServerConfig;

class Connection : public Socket {
public:
    enum class State { READING_HEADERS, READING_BODY, PARSING, SENDING, CLOSED };

private:
    std::string clientIp_;
    std::string receiveBuffer_;
    std::string sendBuffer_;
    State currentState_;
    time_t lastActivity_;
    const ServerConfig* serverConfig_;

public:
    Connection(int fd, const std::string& ip, const ServerConfig* config);
    virtual ~Connection() = default;

    const std::string& getClientIp() const { return clientIp_; }
    State getState() const { return currentState_; }

    //get server configuration to access rule sets
    const ServerConfig* getServerConfig() const { return serverConfig_; }

    // used by server to pull raw text, to pass onto http parser
    const std::string& getReceiveBuffer() const { return receiveBuffer_; }

    // check for \r\n\r\n sequence to indicate full header received
    bool hasCompleteHeaders() const;

    // called by server, consumes bytes which were parsed by http parser
    void consumeReceiveBuffer(size_t bytes);

    // called by server, appends HTTP response string to sendBuffer_
    void appendResponse(const std::string& response);

    //called by server when status == POLLIN, calls recv() and appends to receiveBuffer_
    bool receiveRequest();

    //called by server when status == POLLOUT, calls send() and removes bytes from sendBuffer_
    bool sendResponse();
    bool hasTimedOut(time_t currentTime, int timeoutSeconds) const;
};

/*
    TODO: FUTURE CONNECTION EXTENSIONS?

    1. PARSING: Integrate teammate's HttpRequest class to store parsed data.
    2. ROUTING: Implement LocationConfig matching (Longest Prefix Match).
    3. SECURITY: Enforce client_max_body_size early (after header phase).
    4. METHODS: Validate HttpMethod against allowedMethods in LocationConfig.
    5. POST: Handle 'Transfer-Encoding: chunked' and large body disk-buffering.
    6. REDIR: Handle 'return' directives for 301/302 redirects.
    7. STATIC: Support Autoindex (directory listing) and root/index resolution.
    8. CGI: Implement fork/exec/pipe logic for dynamic script execution.
    9. ERRORS: Resolve custom error_page paths from ServerConfig.
 */
