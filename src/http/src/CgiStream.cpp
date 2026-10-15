#include "CgiStream.hpp"

#include <array>
#include <charconv>
#include <limits>
#include <utility>
#include <variant>

#include "HttpSyntax.hpp"
#include "http/CgiResponseParser.hpp"
#include "http/HttpStatus.hpp"

namespace {
    constexpr int kChunkBase = 16;
    constexpr std::size_t kBitsPerHexDigit = 4;
    constexpr std::size_t kChunkDigits =
        (std::numeric_limits<std::size_t>::digits + kBitsPerHexDigit - 1) / kBitsPerHexDigit;
    constexpr std::string_view kChunkEnd = "0\r\n\r\n";
} // namespace

CgiStream::CgiStream(HttpSerializer::Framing framing) : framing_(framing) {
    framing_.stream = true;
}

bool CgiStream::receive(std::string_view output, std::string& bytes) {
    if (ended_) {
        return false;
    }

    if (parsed_) {
        return append(output, bytes);
    }

    headers_.append(output);
    auto result = CgiResponseParser::parseHeaders(headers_);

    if (std::holds_alternative<CgiResponseParser::Incomplete>(result)) {
        return true;
    }

    auto* headers = std::get_if<CgiResponseParser::Headers>(&result);

    if (headers == nullptr) {
        return false;
    }

    length_ = headers->length;
    const HttpStatus status = headers->response.status;
    suppressed_ = framing_.headersOnly || status == HttpStatus::NoContent || status == HttpStatus::NotModified;
    framing_.chunked = !suppressed_ && !length_;
    parsed_ = true;

    HttpSerializer::Output serialized = HttpSerializer::serialize(std::move(headers->response), framing_);
    bytes.append(serialized.headers);
    const bool valid = append(std::string_view(headers_).substr(headers->consumed), bytes);
    std::string().swap(headers_);
    return valid;
}

bool CgiStream::append(std::string_view output, std::string& bytes) {
    if (suppressed_ || output.empty()) {
        return true;
    }

    if (length_) {
        if (output.size() > *length_ - received_) {
            bytes.append(output.substr(0, static_cast<std::size_t>(*length_ - received_)));
            received_ = *length_;
            return false;
        }

        received_ += output.size();
    }

    if (framing_.chunked) {
        std::array<char, kChunkDigits> digits{};
        const auto result = std::to_chars(digits.data(), digits.data() + digits.size(), output.size(), kChunkBase);
        bytes.append(digits.data(), static_cast<std::size_t>(result.ptr - digits.data()));
        bytes.append(Http::Syntax::CRLF);
    }

    bytes.append(output);

    if (framing_.chunked) {
        bytes.append(Http::Syntax::CRLF);
    }

    return true;
}

bool CgiStream::end(std::string& bytes) {
    if (!parsed_ || ended_ || (!suppressed_ && length_ && received_ != *length_)) {
        return false;
    }

    if (framing_.chunked) {
        bytes.append(kChunkEnd);
    }

    ended_ = true;
    return true;
}

bool CgiStream::hasHeaders() const {
    return parsed_;
}
