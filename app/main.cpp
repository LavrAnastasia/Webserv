#include "config/ConfigLoader.hpp"
#include "log/Log.hpp"
#include "net/EventLoop.hpp"
#include "net/TcpServer.hpp"
#include <cstdlib>
#include <exception>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        Log::error("usage: webserv <config file>");
        return EXIT_FAILURE;
    }

    try {
        const Configuration config = ConfigLoader::load(argv[1]);

        TcpServer server(config);

        EventLoop loop(server);

        EventLoop::setupSignals();

        loop.initialize();

        Log::info("server started, press Ctrl+C to stop");

        try {
            loop.run();
        } catch (const std::exception& error) {
            Log::error(std::string("server stopped: ") + error.what());
            return EXIT_FAILURE;
        }
    } catch (const std::exception& error) {
        Log::error(std::string("cannot start: ") + error.what());
        return EXIT_FAILURE;
    }

    Log::info("server stopped");

    return EXIT_SUCCESS;
}
