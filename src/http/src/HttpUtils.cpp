#include "http/HttpUtils.hpp"

std::string toLowerAscii(const std::string& name) {
    std::string result = name;
    std::size_t index = 0;

    while (index < result.size()) {
        if (result[index] >= 'A' && result[index] <= 'Z') {
            result[index] = static_cast<char>(result[index] - 'A' + 'a');
        }
        ++index;
    }
    return result;
}
