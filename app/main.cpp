#include "config/ConfigLoader.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

namespace {
    void printError(const std::string& message) {
        std::cerr << "webserv: error: " << message << '\n';
    }
} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printError("expected exactly one config file");
        return EXIT_FAILURE;
    }

    try {
        const Config config = ConfigLoader::load(argv[1]);

        (void)config;

        //  Run HTTP Server (app folder class)
        //      Run TCP Server (net module)
        //          Create, Bind, Listen, Register sockets
        //      Run Event Loop (net module)
        //          Runs the infinite event cycle
        //          Waits events from Poller
        //          Create Connection Entity
        //          Stores Connection Entity in ConnectionRegistry
        //          Event readable ? ->
        //              bad/ closed/ not enough data ? skip or do some work
        //              Connection -> get HttpRequest
        //              Request Handler ->  make HttpResponse from HttpRequest
        //              Serialize Response to bytes
        //              make event ready to write
        //          Event Writable?
        //          write
    } catch (const std::exception& error) {
        printError(error.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
