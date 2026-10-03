#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "http/HttpRequest.hpp"

namespace CgiEnvironment {

    std::vector<std::string> build(const HttpRequest& request, const std::filesystem::path& script);

} // namespace CgiEnvironment
