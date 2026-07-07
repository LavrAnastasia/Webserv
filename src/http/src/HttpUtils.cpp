#include <algorithm>
#include <cctype>

#include "HttpSyntax.hpp"
#include "HttpUtils.hpp"

namespace Http::Ascii {
    namespace {
        char toLowerAsciiChar(char c) {
            unsigned char uc = static_cast<unsigned char>(c);

            if (uc >= 'A' && uc <= 'Z') {
                return static_cast<char>(uc - 'A' + 'a');
            }

            return c;
        }

        bool isWhiteSpace(char c) {
            return c == Http::Syntax::SP || c == Http::Syntax::HTAB;
        }
    } // namespace
    std::string tolower(const std::string& value) {
        std::string result = value;

        std::transform(result.begin(), result.end(), result.begin(), [](char c) { return toLowerAsciiChar(c); });

        return result;
    }

    std::string trim(const std::string& value) {
        auto start = std::find_if(value.begin(), value.end(), [](char c) { return !isWhiteSpace(c); });

        if (start == value.end()) {
            return "";
        }

        auto reverseEnd = std::find_if(value.rbegin(), value.rend(), [](char c) { return !isWhiteSpace(c); });

        auto end = reverseEnd.base();

        return std::string(start, end);
    }

} // namespace Http::Ascii
