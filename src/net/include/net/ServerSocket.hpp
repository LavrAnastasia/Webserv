#pragma once

#include "net/Socket.hpp"

#include <cstdint>
#include <netinet/in.h>
#include <string>

class ServerSocket : public Socket {
private:
    std::uint16_t port_; //port to listen on
    struct sockaddr_in socketAddress_{};

public:
    ServerSocket(const std::string& host, std::uint16_t port);

    std::uint16_t getPort() const;

    int acceptConnection(std::string& clientIp, uint16_t& clientPort);
};

/*
    the contents of the sockaddr_in struct:

    struct sockaddr_in {
    short            sin_family;   // Address family (e.g., AF_INET)
    unsigned short   sin_port;     // Port number (e.g., 8080)
    struct in_addr   sin_addr;     // IP address
    char             sin_zero[8];  // Padding to make it the right size
    };

    struct in_addr {
    unsigned long s_addr;          // load with inet_aton()
    };

    Constructors with exactly one parameter should ALWAYS be explicit:
    this makes the constructor only callable with the proper syntax
    (eg. ServerSocket mySocket(8080)), and will not allow the compiler to
    do potentially unintended implicit conversions like
    ServerSocket mySocket = 8080
    With the addition of the second parameter, this is not necessary

    Non-blocking being set in a dedicated method, instead of eg. inside the
    class constructor is considered best practice because of: 1. separation of
    concerns and 2. reusability
    ()
};
*/
