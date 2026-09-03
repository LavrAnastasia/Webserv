#include "UrlCodec.hpp"

namespace {
    bool isUnreserved(unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' ||
            c == '_' || c == '~';
    }

    std::string encode(const std::string& value, bool keepSlash) {
        constexpr char hex[] = "0123456789ABCDEF";

        std::string result;

        for (const unsigned char c : value) {
            if (isUnreserved(c) || (keepSlash && c == '/')) {
                result += static_cast<char>(c);
                continue;
            }

            result += '%';
            result += hex[c >> 4];
            result += hex[c & 0x0F];
        }
        return result;
    }
} // namespace

namespace Http::Url {
    std::string encodePath(const std::string& path) {
        return encode(path, true);
    }

    std::string encodeSegment(const std::string& value) {
        return encode(value, false);
    }
} // namespace Http::Url
