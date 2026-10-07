#include "net/EventLoop.hpp"
#include "http/CgiResponseParser.hpp"
#include "http/HttpResponse.hpp"
#include "http/RequestDispatcher.hpp"
#include "log/Log.hpp"
#include "net/TcpServer.hpp"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <exception>
#include <optional>
#include <string>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>

namespace {
    constexpr std::chrono::seconds kCgiTimeout{60};
    constexpr std::size_t kMaxConnections = 1000;
    constexpr std::size_t kMaxCgi = 128;
    constexpr std::chrono::milliseconds kAcceptPause{500};
} // namespace

volatile std::sig_atomic_t EventLoop::stopRequested_ = 0;

void EventLoop::setupSignals() {
    std::signal(SIGINT, EventLoop::handleSignal);
    std::signal(SIGTERM, EventLoop::handleSignal);
}

void EventLoop::handleSignal(int sig) {
    (void)sig;
    stopRequested_ = 1;
}

void EventLoop::handleNewConnection(int listenFd) {
    const auto accepted = tcpServer_.acceptClient(listenFd);

    if (const TcpServer::AcceptError* error = std::get_if<TcpServer::AcceptError>(&accepted)) {
        if (*error == TcpServer::AcceptError::NoDescriptors) {
            Log::warn("accept() failed: too many open files, pausing new connections");
            acceptAgainAt_ = std::chrono::steady_clock::now() + kAcceptPause;
        }

        updateAcceptEvents();
        return;
    }

    const TcpServer::ClientInfo& clientInfo = std::get<TcpServer::ClientInfo>(accepted);

    //get config block for this fd
    const ServerConfig* config = tcpServer_.getConfigForFd(listenFd);
    if (config) {
        try {
            //add new connection to registry, including config block
            connectionRegistry_.addConnection(clientInfo.fd, clientInfo.ip, clientInfo.serverPort, config);
            //tell poller to track it (watch for incoming http request)
            poller_.addSocket(clientInfo.fd);
        } catch (const std::exception& e) {
            // in case of setNonBlocking() failure, log error and remove connection + socket
            Log::error(std::string("failed to set up connection: ") + e.what() + ", client: " + clientInfo.ip);
            closeConnection(clientInfo.fd);
        }
    } else {
        // if config for accepted client not found, close connection
        close(clientInfo.fd);
    }

    updateAcceptEvents();
}

void EventLoop::handleClientActivity(int clientFd, uint32_t events) {
    //fetch client
    Connection* connection = connectionRegistry_.getConnection(clientFd);
    if (connection == nullptr) {
        return;
    }
    // close on error, or on hangup with nothing left to read
    if ((events & POLLERR) || ((events & POLLHUP) && !(events & POLLIN))) {
        closeConnection(clientFd);
        return;
    }

    if (cgiRegistry_.find(clientFd) != nullptr) {
        if (!connection->isAlive()) {
            closeConnection(clientFd);
        } else {
            poller_.modifySocket(clientFd, 0);
        }
        return;
    }
    //reading phase (OS kernel receive buffer has data)
    if (events & POLLIN) {
        //feed bytes from OS kernel's socket buffer into parser and receive status
        const std::optional<ParseResult> received = connection->receiveRequest();

        // Client disconnected -> clean up immediately
        if (!received) {
            closeConnection(clientFd);
            return;
        }

        const ParseResult& result = *received;

        // Parsing complete -> build response from HttpRequest
        if (const Complete* complete = std::get_if<Complete>(&result)) {
            const ServerConfig& server = connection->getServerConfig();
            HandlerResult handlerResult = RequestDispatcher::dispatch(complete->request, server, connection->info());

            connection->setShouldClose(!complete->request.isPersistent());
            connection->setHeadersOnly(complete->request.method == HttpMethod::Head);

            if (const HttpResponse* response = std::get_if<HttpResponse>(&handlerResult)) {
                connection->appendResponse(*response);
                poller_.modifySocket(clientFd, POLLOUT);
            } else {
                launchCgi(*connection, std::get<CgiRequest>(handlerResult));
            }
        }

        /*
        fail case: unable to parse client request -> build error response and
        close connection after sending!
        */
        else if (const Failed* failed = std::get_if<Failed>(&result)) {
            connection->setShouldClose(true);
            connection->setHeadersOnly(false);
            connection->appendResponse(RequestDispatcher::fail(failed->status, connection->getServerConfig()));
            poller_.modifySocket(clientFd, POLLOUT); //switch to POLLOUT to send error
        }
        /*
            case 'NeedMoreData' -> do nothing and wait for next loop - no POLLOUT switch
        */
    }

    //send phase (OS write bucket has space)
    if (events & POLLOUT) {
        if (!connection->sendResponse()) {
            //client disconnected
            closeConnection(clientFd);
            return;
        }

        if (connection->isSendComplete()) {
            if (connection->shouldClose()) {
                closeConnection(clientFd);
            } else {
                connection->resetParser();
                /*
                    TODO: HTTP pipelining support:
                    If client has sent multiple requests and parser buffer still
                    has data in it after reset, parser should be re-run immediately.
                    Requests already in buffer need to be processed before setting
                    socket to POLLIN.
                */
                poller_.modifySocket(clientFd, POLLIN);
            }
        }
    }
}

void EventLoop::launchCgi(Connection& connection, const CgiRequest& request) {
    if (cgiRegistry_.size() >= kMaxCgi) {
        Log::warn("too many CGI processes running, client: " + connection.info().remoteAddr);
        connection.appendResponse(
            RequestDispatcher::fail(HttpStatus::ServiceUnavailable, connection.getServerConfig())
        );
        poller_.modifySocket(connection.getFd(), POLLOUT);
        return;
    }

    std::optional<CgiProcess> process = CgiProcess::launch(request);

    if (!process) {
        Log::error(
            "failed to start CGI " + request.script.string() + " with " + request.interpreter.string() +
            ", client: " + connection.info().remoteAddr
        );
        connection.appendResponse(RequestDispatcher::fail(HttpStatus::BadGateway, connection.getServerConfig()));
        poller_.modifySocket(connection.getFd(), POLLOUT);
        return;
    }

    if (process->inputFd() >= 0) {
        poller_.addSocket(process->inputFd());
        poller_.modifySocket(process->inputFd(), POLLOUT);
    }

    poller_.addSocket(process->outputFd());
    poller_.modifySocket(connection.getFd(), POLLIN);
    cgiRegistry_.add(connection.getFd(), std::move(*process));
}

void EventLoop::handleCgiActivity(int clientFd, int pipeFd) {
    CgiProcess* process = cgiRegistry_.find(clientFd);

    if (process == nullptr) {
        return;
    }

    const int input = process->inputFd();
    const int output = process->outputFd();

    if (pipeFd == input) {
        process->writeInput();
    } else if (pipeFd == output) {
        process->readOutput();
    } else {
        return;
    }

    if (process->inputFd() != input) {
        poller_.removeSocket(input);
    }

    if (process->outputFd() != output) {
        poller_.removeSocket(output);
    }

    if (process->isAlive()) {
        return;
    }

    Connection* connection = connectionRegistry_.getConnection(clientFd);

    if (connection == nullptr) {
        cgiRegistry_.remove(clientFd);
        return;
    }

    const std::optional<HttpResponse> response = CgiResponseParser::parse(process->output());

    if (response) {
        connection->appendResponse(*response);
    } else {
        const std::string problem = process->output().empty() ? "CGI exited without output" : "CGI sent invalid header";
        Log::error(problem + " while reading response header, client: " + connection->info().remoteAddr);
        connection->appendResponse(RequestDispatcher::fail(HttpStatus::BadGateway, connection->getServerConfig()));
    }

    cgiRegistry_.remove(clientFd);
    poller_.modifySocket(clientFd, POLLOUT);
}

void EventLoop::closeCgi(int clientFd) {
    if (CgiProcess* process = cgiRegistry_.find(clientFd)) {
        poller_.removeSocket(process->inputFd());
        poller_.removeSocket(process->outputFd());
        cgiRegistry_.remove(clientFd);
    }
}

void EventLoop::closeConnection(int fd) {
    closeCgi(fd);
    poller_.removeSocket(fd);
    connectionRegistry_.removeConnection(fd);
}

void EventLoop::initialize() {
    std::signal(SIGPIPE, SIG_IGN);

    listeningFds_ = tcpServer_.getListeningFds();
    for (int fd : listeningFds_) {
        poller_.addSocket(fd);
    }
}

void EventLoop::run() {
    while (stopRequested_ == 0) {
        // poller returns vector<pollfd> of active sockets (incl. *what* activity)
        auto activeSockets = poller_.waitForEvents();
        for (pollfd& event : activeSockets) {
            const bool listening = std::ranges::find(listeningFds_, event.fd) != listeningFds_.end();
            const std::optional<int> client = listening ? std::nullopt : cgiRegistry_.client(event.fd);

            try {
                if (listening) {
                    handleNewConnection(event.fd);
                } else if (client) {
                    handleCgiActivity(*client, event.fd);
                } else {
                    handleClientActivity(event.fd, event.revents);
                }
            } catch (const std::exception& error) {
                if (listening) {
                    Log::error(std::string("failed to accept connection: ") + error.what());
                    continue;
                }

                const int fd = client.value_or(event.fd);
                const Connection* connection = connectionRegistry_.getConnection(fd);
                const std::string ip = connection != nullptr ? connection->info().remoteAddr : "unknown";

                closeConnection(fd);
                Log::error(std::string(error.what()) + " while processing request, connection closed, client: " + ip);
            }
        }

        try {
            cleanupTimedOutConnections();
            cleanupTimedOutCgi();
        } catch (const std::exception& error) {
            Log::error(std::string(error.what()) + " while checking timeouts");
        }

        updateAcceptEvents();
    }
}

void EventLoop::cleanupTimedOutConnections() {
    std::vector<int> timedOutFds =
        connectionRegistry_.getTimedOutConnections(clientTimeoutSeconds_, std::chrono::steady_clock::now());

    for (int fd : timedOutFds) {
        Connection* connection = connectionRegistry_.getConnection(fd);

        if (!connection)
            continue;

        if (cgiRegistry_.find(fd)) {
            continue;
        }

        // response pending or connection flagged to close: close immediately, do not queue 408
        if (connection->shouldClose() || !connection->isSendComplete()) {
            Log::info("client timed out while sending response, client: " + connection->info().remoteAddr);
            closeConnection(fd);
            continue;
        }

        // no response pending: client timed out while sending request
        connection->setShouldClose(true);
        connection->appendResponse(RequestDispatcher::fail(HttpStatus::RequestTimeout, connection->getServerConfig()));
        poller_.modifySocket(fd, POLLOUT);
        Log::info("client timed out while waiting for request, client: " + connection->info().remoteAddr);
    }
}

void EventLoop::cleanupTimedOutCgi() {
    for (int clientFd : cgiRegistry_.expired(kCgiTimeout, std::chrono::steady_clock::now())) {
        closeCgi(clientFd);

        Connection* connection = connectionRegistry_.getConnection(clientFd);

        if (connection == nullptr) {
            continue;
        }

        Log::error(
            "CGI timed out after " + std::to_string(kCgiTimeout.count()) +
            " s while reading response, client: " + connection->info().remoteAddr
        );
        connection->appendResponse(RequestDispatcher::fail(HttpStatus::GatewayTimeout, connection->getServerConfig()));
        poller_.modifySocket(clientFd, POLLOUT);
    }
}

void EventLoop::updateAcceptEvents() {
    const bool enabled =
        connectionRegistry_.size() < kMaxConnections && std::chrono::steady_clock::now() >= acceptAgainAt_;

    if (enabled == acceptEventsEnabled_) {
        return;
    }

    acceptEventsEnabled_ = enabled;

    for (int fd : listeningFds_) {
        poller_.modifySocket(fd, enabled ? POLLIN : 0);
    }
}
