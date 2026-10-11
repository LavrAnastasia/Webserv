#include "net/EventLoop.hpp"
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
    ConnectionRegistry::Client* client = connectionRegistry_.find(clientFd);

    if (client == nullptr) {
        return;
    }

    Connection& connection = client->connection;

    // close on error, or on hangup with nothing left to read
    if ((events & POLLERR) || ((events & POLLHUP) && !(events & POLLIN))) {
        closeConnection(clientFd);
        return;
    }

    if (cgiRegistry_.find(clientFd) != nullptr) {
        if (!connection.isAlive()) {
            Log::info("client closed connection while CGI was running, client: " + connection.info().remoteAddr);
            closeConnection(clientFd);
        } else {
            poller_.modifySocket(clientFd, 0);
        }
        return;
    }

    //reading phase (OS kernel receive buffer has data)
    if (events & POLLIN) {
        const std::optional<std::string_view> bytes = connection.receive();

        // client disconnected or the socket failed
        if (!bytes) {
            closeConnection(clientFd);
            return;
        }

        execute(clientFd, client->session.receive(*bytes));
        return;
    }

    //send phase (OS write bucket has space)
    if (events & POLLOUT) {
        if (!connection.send()) {
            closeConnection(clientFd);
            return;
        }

        if (connection.pending() == 0) {
            execute(clientFd, client->session.emit());
        }
    }
}

void EventLoop::execute(int clientFd, HttpSession::Action action) {
    ConnectionRegistry::Client* client = connectionRegistry_.find(clientFd);

    if (client == nullptr) {
        return;
    }

    // TODO: WEB-55 a Write while the CGI streams keeps it running and pauses its output pipe
    if (!std::holds_alternative<HttpSession::ReadOutput>(action)) {
        closeCgi(clientFd);
    }

    if (std::holds_alternative<HttpSession::Close>(action)) {
        closeConnection(clientFd);
    } else if (HttpSession::Write* write = std::get_if<HttpSession::Write>(&action)) {
        client->connection.queue(std::move(write->bytes));
        poller_.modifySocket(clientFd, POLLOUT);
    } else if (const CgiRequest* request = std::get_if<CgiRequest>(&action)) {
        launchCgi(*client, *request);
    } else {
        poller_.modifySocket(clientFd, POLLIN);
    }
}

void EventLoop::launchCgi(ConnectionRegistry::Client& client, const CgiRequest& request) {
    const int clientFd = client.connection.getFd();

    if (cgiRegistry_.size() >= kMaxCgi) {
        Log::warn("too many CGI processes running, client: " + client.connection.info().remoteAddr);
        execute(clientFd, client.session.fail(HttpSession::CgiFailure::Busy));
        return;
    }

    std::optional<CgiProcess> process = CgiProcess::launch(request);

    if (!process) {
        Log::error(
            "failed to start CGI " + request.script.string() + " with " + request.interpreter.string() +
            ", client: " + client.connection.info().remoteAddr
        );
        execute(clientFd, client.session.fail(HttpSession::CgiFailure::Launch));
        return;
    }

    if (process->inputFd() >= 0) {
        poller_.addSocket(process->inputFd());
        poller_.modifySocket(process->inputFd(), POLLOUT);
    }

    poller_.addSocket(process->outputFd());
    poller_.modifySocket(clientFd, POLLIN);
    cgiRegistry_.add(clientFd, std::move(*process));
}

void EventLoop::handleCgiActivity(int clientFd, int pipeFd) {
    CgiProcess* process = cgiRegistry_.find(clientFd);
    ConnectionRegistry::Client* client = connectionRegistry_.find(clientFd);

    if (process == nullptr) {
        return;
    }

    if (client == nullptr) {
        closeCgi(clientFd);
        return;
    }

    if (pipeFd == process->inputFd()) {
        process->writeInput();

        if (process->inputFd() != pipeFd) {
            poller_.removeSocket(pipeFd);
        }
        return;
    }

    if (pipeFd != process->outputFd()) {
        return;
    }

    std::string bytes;

    switch (process->readOutput(bytes)) {
        case CgiProcess::Output::Data:
            execute(clientFd, client->session.receiveOutput(bytes));
            break;
        case CgiProcess::Output::End:
            closeCgi(clientFd);
            execute(clientFd, client->session.endOutput());
            break;
        case CgiProcess::Output::Error:
            Log::error("failed to read CGI output, client: " + client->connection.info().remoteAddr);
            closeCgi(clientFd);
            execute(clientFd, client->session.fail(HttpSession::CgiFailure::Output));
            break;
    }
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
                const ConnectionRegistry::Client* failed = connectionRegistry_.find(fd);
                const std::string ip = failed != nullptr ? failed->connection.info().remoteAddr : "unknown";

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
        ConnectionRegistry::Client* client = connectionRegistry_.find(fd);

        if (client == nullptr || cgiRegistry_.find(fd)) {
            continue;
        }

        execute(fd, client->session.expire());
    }
}

void EventLoop::cleanupTimedOutCgi() {
    for (int clientFd : cgiRegistry_.expired(kCgiTimeout, std::chrono::steady_clock::now())) {
        closeCgi(clientFd);

        ConnectionRegistry::Client* client = connectionRegistry_.find(clientFd);

        if (client == nullptr) {
            continue;
        }

        Log::error(
            "CGI timed out after " + std::to_string(kCgiTimeout.count()) +
            " s while reading response, client: " + client->connection.info().remoteAddr
        );
        execute(clientFd, client->session.fail(HttpSession::CgiFailure::Timeout));
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
