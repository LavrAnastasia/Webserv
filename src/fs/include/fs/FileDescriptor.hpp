#pragma once

class FileDescriptor {
private:
    int fd_;

public:
    FileDescriptor();
    explicit FileDescriptor(int fd);
    ~FileDescriptor();

    FileDescriptor(FileDescriptor&& other) noexcept;
    FileDescriptor& operator=(FileDescriptor&& other) noexcept;
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    int get() const;
    bool isOpen() const;
    void close();
    bool setNonBlocking();
    bool setCloseOnExec();
};
