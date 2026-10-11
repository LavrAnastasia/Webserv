#include "http/CgiResponse.hpp"

#include <array>
#include <charconv>
#include <utility>
#include <variant>

#include "http/CgiResponseParser.hpp"
#include "http/HttpSerializer.hpp"

namespace {
    constexpr std::size_t kBufferSize = 64 * 1024;
    constexpr std::size_t kChunkOverhead = 32;
} // namespace

CgiResponse::CgiResponse(bool close, bool headersOnly) : close_(close), headersOnly_(headersOnly) {
}

std::size_t CgiResponse::capacity() const {
    if (finished_) {
        return 0;
    }

    if (!parsed_) {
        return CgiResponseParser::headerLimit() - headers_.size();
    }

    if (suppressed_) {
        return kBufferSize;
    }

    const std::size_t pending = buffer_.size() - offset_;
    return pending + kChunkOverhead < kBufferSize ? kBufferSize - pending - kChunkOverhead : 0;
}

bool CgiResponse::append(std::string_view bytes) {
    if (finished_ || bytes.size() > capacity()) {
        return false;
    }

    if (parsed_) {
        return appendBody(bytes);
    }

    headers_.append(bytes);
    auto result = CgiResponseParser::parseHeaders(headers_);

    if (std::holds_alternative<CgiResponseParser::Incomplete>(result)) {
        return true;
    }

    auto* head = std::get_if<CgiResponseParser::Head>(&result);

    if (head == nullptr) {
        return false;
    }

    length_ = head->length;
    auto output = HttpSerializer::serialize(
        std::move(head->response), {.close = close_, .headersOnly = headersOnly_, .stream = true}
    );
    chunked_ = output.chunked;
    suppressed_ = output.suppressed;
    buffer_ = std::move(output.headers);
    parsed_ = true;

    const bool valid = appendBody(std::string_view(headers_).substr(head->consumed));
    std::string().swap(headers_);
    return valid;
}

bool CgiResponse::appendBody(std::string_view bytes) {
    if (suppressed_ || bytes.empty()) {
        return true;
    }

    if (length_) {
        if (bytes.size() > *length_ - received_) {
            return false;
        }
        received_ += bytes.size();
    }

    compact();

    if (chunked_) {
        std::array<char, 2 * sizeof(std::size_t)> digits{};
        const auto result = std::to_chars(digits.data(), digits.data() + digits.size(), bytes.size(), 16);
        buffer_.append(digits.data(), static_cast<std::size_t>(result.ptr - digits.data()));
        buffer_.append("\r\n");
    }

    buffer_.append(bytes);

    if (chunked_) {
        buffer_.append("\r\n");
    }

    return true;
}

bool CgiResponse::finish() {
    if (!parsed_ || finished_ || (!suppressed_ && length_ && received_ != *length_)) {
        return false;
    }

    compact();
    if (chunked_) {
        buffer_.append("0\r\n\r\n");
    }
    finished_ = true;
    return true;
}

std::string_view CgiResponse::data() const {
    return std::string_view(buffer_).substr(offset_);
}

void CgiResponse::consume(std::size_t count) {
    offset_ += count;
    started_ = started_ || count != 0;

    if (offset_ == buffer_.size()) {
        buffer_.clear();
        offset_ = 0;
    }
}

void CgiResponse::compact() {
    if (offset_ != 0) {
        buffer_.erase(0, offset_);
        offset_ = 0;
    }
}

bool CgiResponse::started() const {
    return started_;
}

bool CgiResponse::done() const {
    return finished_ && data().empty();
}
