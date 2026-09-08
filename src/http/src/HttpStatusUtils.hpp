#pragma once

#include <http/HttpStatus.hpp>
#include <string>
#include <system_error>

namespace Http::Status {
    std::string toString(HttpStatus status);
    HttpStatus from(const std::error_code& error);
} // namespace Http::Status
