#pragma once

#include <netinet/in.h>
#include <string>

class ServerSocket {
private:
    int fd_; //int representing open socket
    int port_; //port to listen on
    struct sockaddr_in address_;

public:
    ServerSocket(const std::string& host, int port);
    ~ServerSocket();

    int getFd() const;
    int getPort() const;
    void setNonBlocking();
};

/*
    the contents of the sockaddr_in struct:

    struct sockaddr_in {
    short            sin_family;   // Address family (e.g., AF_INET)
    unsigned short   sin_port;     // Port number (e.g., 8080)
    struct in_addr   sin_addr;     // IP address
    char             sin_zero[8];  // Padding to make it the right size

    Constructors with exactly one parameter should ALWAYS be explicit:
    this makes the constructor only callable with the proper syntax
    (eg. ServerSocket mySocket(8080)), and will not allow the compiler to
    do potentially unintended implicit conversions like
    ServerSocket mySocket = 8080
    With the addition of the second parameter, this is not necessary

    Manual destructor necessary, because the port corresponding to fd_ needs
    to be closed when the object is destroyed.

    Non-blocking being set in a dedicated method, instead of eg. inside the
    class constructor is considered best practice because of: 1. separation of
    concerns and 2. reusability
    ()
};
*/
