#include "config/ConfigLoader.hpp"
#include "net/EventLoop.hpp"
#include "net/TcpServer.hpp"
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
        const Configuration config = ConfigLoader::load(argv[1]);

        TcpServer server(config);

        EventLoop loop(server);

        EventLoop::registerSignalHandler(&loop);

        loop.initialize();

        std::cout << "webserv: info: server listening -press Ctrl+C to shut down" << std::endl;

        loop.run();

        //             Request Handler ->  make HttpResponse from HttpRequest
        //             Serialize Response to bytes

    } catch (const std::exception& error) {
        printError(error.what());
        return EXIT_FAILURE;
    }

    std::cout << "webserv: info: graceful shutdown complete" << std::endl;

    return EXIT_SUCCESS;
}
