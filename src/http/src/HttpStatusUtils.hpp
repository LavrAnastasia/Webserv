#pragma once

#include <http/HttpStatus.hpp>
#include <string>

namespace Http::Status {
    std::string toString(HttpStatus status);
} // namespace Http::Status
