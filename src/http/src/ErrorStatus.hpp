#pragma once

#include <system_error>

#include "http/HttpStatus.hpp"

namespace Http::Status {

    HttpStatus from(const std::error_code& error);

} // namespace Http::Status
