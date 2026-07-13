
#pragma once

#include <string_view>

namespace Http::Headers {
    constexpr std::string_view ContentLength = "content-length";
    constexpr std::string_view TransferEncoding = "transfer-encoding";
    constexpr std::string_view ChunkedValue = "chunked";
    constexpr std::string_view Host = "host";
} // namespace Http::Headers
