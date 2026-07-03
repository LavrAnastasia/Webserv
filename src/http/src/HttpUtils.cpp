#include "HttpUtils.hpp"

#include <algorithm>
#include <cctype>

namespace Http::Ascii {
    std::string tolower(const std::string& value) {
        std::string result = value;

        std::transform(result.begin(), result.end(), result.begin(), [](char c) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        });
        return result;
    }

    std::string trim(const std::string& value) {
        std::string::const_iterator start = std::find_if(value.begin(), value.end(), [](char c) {
            return !std::isspace(static_cast<unsigned char>(c));
        });
    
        if (start == value.end()) {
            return "";
        }
    
        std::string::const_reverse_iterator reverseEnd = std::find_if(value.rbegin(), value.rend(), [](char c) {
            return !std::isspace(static_cast<unsigned char>(c));
        });
    
        std::string::const_iterator end = reverseEnd.base();
    
        return std::string(start, end);
    }

} // namespace Http::Ascii
