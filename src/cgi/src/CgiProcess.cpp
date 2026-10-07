#include "cgi/CgiProcess.hpp"

#include <array>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace {
    constexpr std::size_t kMaxOutputSize = 10 * 1024 * 1024; // 10 MB
    constexpr std::size_t kReadChunkSize = 64 * 1024; // 64 KB
    constexpr int kExecFailed = 127;

    struct Pipe {
        FileDescriptor readEnd;
        FileDescriptor writeEnd;
    };

    std::optional<Pipe> openPipe() {
        std::array<int, 2> ends{};

        if (::pipe(ends.data()) == -1) {
            return std::nullopt;
        }

        Pipe result{FileDescriptor(ends[0]), FileDescriptor(ends[1])};

        if (!result.readEnd.setCloseOnExec() || !result.writeEnd.setCloseOnExec()) {
            return std::nullopt;
        }

        return result;
    }

    std::vector<char*> pointers(std::vector<std::string>& strings) {
        std::vector<char*> result;

        result.reserve(strings.size() + 1);

        for (std::string& string : strings) {
            result.push_back(string.data());
        }

        result.push_back(nullptr);

        return result;
    }
} // namespace

CgiProcess::CgiProcess(pid_t pid, FileDescriptor inputPipe, FileDescriptor outputPipe, std::string body)
    : pid_(pid), alive_(true), inputPipe_(std::move(inputPipe)), outputPipe_(std::move(outputPipe)),
      body_(std::move(body)), written_(0), output_() {
    if (body_.empty()) {
        inputPipe_.close();
    }
}

CgiProcess::~CgiProcess() {
    kill();
}

CgiProcess::CgiProcess(CgiProcess&& other) noexcept
    : pid_(std::exchange(other.pid_, -1)), alive_(std::exchange(other.alive_, false)),
      inputPipe_(std::move(other.inputPipe_)), outputPipe_(std::move(other.outputPipe_)), body_(std::move(other.body_)),
      written_(other.written_), output_(std::move(other.output_)) {
}

CgiProcess& CgiProcess::operator=(CgiProcess&& other) noexcept {
    if (this != &other) {
        kill();

        pid_ = std::exchange(other.pid_, -1);
        alive_ = std::exchange(other.alive_, false);
        inputPipe_ = std::move(other.inputPipe_);
        outputPipe_ = std::move(other.outputPipe_);
        body_ = std::move(other.body_);
        written_ = other.written_;
        output_ = std::move(other.output_);
    }

    return *this;
}

std::optional<CgiProcess> CgiProcess::launch(const CgiRequest& request) {
    std::error_code error;

    if (!std::filesystem::is_regular_file(request.interpreter, error) ||
        ::access(request.interpreter.c_str(), X_OK) != 0) {
        return std::nullopt;
    }

    std::optional<Pipe> input = openPipe();
    std::optional<Pipe> output = openPipe();

    if (!input || !output || !input->writeEnd.setNonBlocking() || !output->readEnd.setNonBlocking()) {
        return std::nullopt;
    }

    const std::string directory = request.script.parent_path().string();
    std::vector<std::string> arguments = {request.interpreter.string(), request.script.string()};
    std::vector<std::string> environment = request.env;
    std::vector<char*> argv = pointers(arguments);
    std::vector<char*> envp = pointers(environment);

    const pid_t pid = ::fork();

    if (pid == -1) {
        return std::nullopt;
    }

    if (pid == 0) {
        if (::dup2(input->readEnd.get(), STDIN_FILENO) != -1 && ::dup2(output->writeEnd.get(), STDOUT_FILENO) != -1 &&
            ::chdir(directory.c_str()) != -1) {
            ::execve(argv[0], argv.data(), envp.data());
        }

        std::_Exit(kExecFailed);
    }

    return CgiProcess(pid, std::move(input->writeEnd), std::move(output->readEnd), request.body);
}

int CgiProcess::inputFd() const {
    return inputPipe_.get();
}

int CgiProcess::outputFd() const {
    return outputPipe_.get();
}

void CgiProcess::writeInput() {
    if (!inputPipe_.isOpen()) {
        return;
    }

    const ssize_t count = ::write(inputPipe_.get(), body_.data() + written_, body_.size() - written_);

    if (count < 0) {
        inputPipe_.close();
        return;
    }

    written_ += static_cast<std::size_t>(count);

    if (written_ == body_.size()) {
        inputPipe_.close();
    }
}

void CgiProcess::readOutput() {
    if (!outputPipe_.isOpen()) {
        return;
    }

    std::array<char, kReadChunkSize> buffer{};
    const ssize_t count = ::read(outputPipe_.get(), buffer.data(), buffer.size());

    if (count < 0) {
        return;
    }

    if (count == 0) {
        kill();
        return;
    }

    output_.append(buffer.data(), static_cast<std::size_t>(count));

    if (output_.size() > kMaxOutputSize) {
        output_.clear();
        kill();
    }
}

bool CgiProcess::isAlive() const {
    return alive_;
}

const std::string& CgiProcess::output() const {
    return output_;
}

void CgiProcess::reap() {
    if (alive_ && ::waitpid(pid_, nullptr, WNOHANG) != 0) {
        alive_ = false;
    }
}

void CgiProcess::kill() {
    inputPipe_.close();
    outputPipe_.close();
    reap();

    if (alive_) {
        ::kill(pid_, SIGKILL);
        ::waitpid(pid_, nullptr, 0);
        alive_ = false;
    }
}
