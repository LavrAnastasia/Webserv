#pragma once

#include "http/HttpParser.hpp"
#include "net/Socket.hpp"
#include <chrono>
#include <string>

/*
    The connection class:
    This class is the bridge between the OS and high-level HTTP logic.
    It receives the chopped-up TCP transmission sent by the client and feeds
    the pieces to its internal parser until the request is ready to retrieve.

    Tasks:
        1.  handle data: feeds the pieces of a HTTP request to its internal parser
            until the whole request has arrived.
        2.  manage id: stores socket fd and IP address of the client
        3.  track activity: monitors I/O progress to determine when reading or
            writing is complete.

    Inputs:
        1.  From client (via OS): raw bytes to receive with C system call recv()
        2.  From server logic: HTTP response string like "HTTP/1.1 200 OK\r\n\r\n<html>..."

    Outputs:
        1.  To server:  complete, validated HttpRequest object (which the parser
                        fills and provides to the logic module)
        2.  To client (via OS): HTTP response from server, converted into raw bytes
            and sent with C system call send()

    Interacts with :
        1.  Poller (indirectly): when poller tells server that a socket has data,
                server tells corresponding connection to call receiveRequest()
        2.  HTTP parser (directly): the connection feeds raw bytes into the parser
                instance and returns the result of each append operation
        3. Event loop: event loop tracks the lifecycle of each connection and cleans up
                upon completion or fatal error
*/

//forward declaration sufficient for pointer
struct ServerConfig;

class Connection : public Socket {
private:
    std::string clientIp_;
    HttpParser parser_;
    std::string sendBuffer_;
    const ServerConfig& serverConfig_;
    std::chrono::steady_clock::time_point lastActivity_;
    bool shouldClose_;

public:
    Connection(int fd, const std::string& ip, const ServerConfig& config);

    const std::string& getClientIp() const { return clientIp_; }

    //get server configuration to access rule sets
    const ServerConfig& getServerConfig() const { return serverConfig_; }

    //allows EventLoop to access parser_ if necessary
    const HttpParser& getParser() const { return parser_; }
    void setShouldClose(bool state) { shouldClose_ = state; }
    bool shouldClose() const { return shouldClose_; }
    void resetParser() { parser_ = HttpParser(); }

    // used by EventLoop to determine when to switch between POLLOUT and POLLIN
    bool isSendComplete() const { return sendBuffer_.empty(); }

    // called by server, appends HTTP response string to sendBuffer_
    void appendResponse(const std::string& response);

    //called by server when POLLIN detected -reads raw bytes from socket -> HttpParser
    ParseResult receiveRequest();

    //called by server when status == POLLOUT, calls send() and removes bytes from sendBuffer_
    bool sendResponse();
    bool hasTimedOut(std::chrono::steady_clock::time_point currentTime, int timeoutSeconds) const;
};

//added bool shouldClose_
//added helpers setShouldClose(), shouldClose()
//added resetParser()
