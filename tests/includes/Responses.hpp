#pragma once

#include "http/HttpSerializer.hpp"

#include <gtest/gtest.h>
#include <string>
#include <utility>

namespace Responses {
    inline std::string read(ResponseBody& body) {
        std::string result;
        std::string chunk;

        while (!body.done()) {
            if (!body.next(chunk, 16 * 1024) || chunk.empty()) {
                ADD_FAILURE() << "Response body ended before its declared size";
                break;
            }

            result.append(chunk);
        }

        return result;
    }

    inline std::string serialize(HttpResponse response, const HttpSerializer::Framing& framing) {
        auto output = HttpSerializer::serialize(std::move(response), framing);
        std::string result = std::move(output.headers);

        if (output.body) {
            result.append(read(*output.body));
        }

        return result;
    }
} // namespace Responses
