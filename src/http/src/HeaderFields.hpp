#pragma once

#include <string_view>

namespace Http::Headers {
    constexpr std::string_view Connection = "Connection";
    constexpr std::string_view ContentLength = "Content-Length";
    constexpr std::string_view ContentType = "Content-Type";
    constexpr std::string_view TransferEncoding = "Transfer-Encoding";
    constexpr std::string_view Expect = "Expect";
    constexpr std::string_view Host = "Host";
    constexpr std::string_view Date = "Date";
    constexpr std::string_view Allow = "Allow";
    constexpr std::string_view Location = "Location";
    constexpr std::string_view Server = "Server";
    constexpr std::string_view Status = "Status";
    constexpr std::string_view Proxy = "Proxy";
    constexpr std::string_view KeepAlive = "Keep-Alive";
    constexpr std::string_view ProxyConnection = "Proxy-Connection";
    constexpr std::string_view TE = "TE";
    constexpr std::string_view Trailer = "Trailer";
    constexpr std::string_view Upgrade = "Upgrade";

    namespace Expectation {
        constexpr std::string_view Continue = "100-continue";
    }

    namespace ConnectionOption {
        constexpr std::string_view Close = "close";
    }

    namespace TransferCoding {
        constexpr std::string_view Chunked = "chunked";
    }
} // namespace Http::Headers
