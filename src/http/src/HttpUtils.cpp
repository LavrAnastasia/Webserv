#include "HttpUtils.hpp"

namespace Http::Ascii {
    std::string tolower(const std::string& value) {
        std::string result = value;
        std::size_t index = 0;

        while (index < result.size()) {
            if (result[index] >= 'A' && result[index] <= 'Z') {
                result[index] = static_cast<char>(result[index] - 'A' + 'a');
            }
            ++index;
        }
        return result;
    }

    std::string trim(const std::string& value) {
        std::size_t start = 0;
        std::size_t end = value.size();

        while (start < end && (value[start] == ' ' || value[start] == '\t')) {
            ++start;
        }

        while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t')) {
            --end;
        }

        return value.substr(start, end - start);
    }
} // namespace Http::Ascii
