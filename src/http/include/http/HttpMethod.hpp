#pragma once

#include <optional>
#include <string>

enum class HttpMethod { Get, Post, Delete };

std::optional<HttpMethod> parseHttpMethod(const std::string& value);
std::string toString(HttpMethod method);
