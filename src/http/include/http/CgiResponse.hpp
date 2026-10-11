#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

class CgiResponse {
public:
    CgiResponse(bool close, bool headersOnly);

    std::size_t capacity() const;
    bool append(std::string_view bytes);
    bool finish();
    std::string_view data() const;
    void consume(std::size_t count);
    bool started() const;
    bool done() const;

private:
    bool appendBody(std::string_view bytes);
    void compact();

    std::string headers_;
    std::string buffer_;
    std::size_t offset_ = 0;
    std::optional<std::uintmax_t> length_;
    std::uintmax_t received_ = 0;
    bool close_;
    bool headersOnly_;
    bool parsed_ = false;
    bool chunked_ = false;
    bool suppressed_ = false;
    bool finished_ = false;
    bool started_ = false;
};
