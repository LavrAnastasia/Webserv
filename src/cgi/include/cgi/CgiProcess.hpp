#pragma once

#include <optional>
#include <string>

#include <sys/types.h>

#include "fs/FileDescriptor.hpp"
#include "http/CgiRequest.hpp"

class CgiProcess {
private:
    CgiProcess(pid_t pid, FileDescriptor inputPipe, FileDescriptor outputPipe, std::string body);

    pid_t pid_;
    FileDescriptor inputPipe_;
    FileDescriptor outputPipe_;
    std::string body_;
    std::size_t written_;
    std::string output_;

public:
    static std::optional<CgiProcess> launch(const CgiRequest& request);

    ~CgiProcess();

    CgiProcess(CgiProcess&& other) noexcept;
    CgiProcess& operator=(CgiProcess&& other) noexcept;
    CgiProcess(const CgiProcess&) = delete;
    CgiProcess& operator=(const CgiProcess&) = delete;

    int inputFd() const;
    int outputFd() const;

    void writeInput();
    void readOutput();

    bool isAlive() const;
    const std::string& output() const;
};
