#pragma once

#include <string_view>

namespace Http::Syntax {
    constexpr char SP = ' ';
    constexpr char HTAB = '\t';
    constexpr char CR = '\r';
    constexpr char LF = '\n';

    constexpr std::string_view CRLF = "\r\n";
    constexpr std::string_view HeaderSectionEnd = "\r\n\r\n";
    constexpr char HeaderKeyEnd = ':';
    constexpr char ChunkExtSeparator = ';';
    constexpr char QuerySeparator = '?';
    constexpr char PathPrefix = '/';

} // namespace Http::Syntax
