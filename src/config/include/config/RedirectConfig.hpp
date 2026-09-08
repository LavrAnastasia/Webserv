#pragma once

#include "http/HttpStatus.hpp"

#include <optional>
#include <string>

struct RedirectConfig {
    HttpStatus status;
    std::optional<std::string> target;
};
