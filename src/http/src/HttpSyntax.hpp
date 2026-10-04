#pragma once

#include <cstddef>
#include <string_view>

namespace Http::Syntax {
    constexpr char SP = ' ';
    constexpr char HTAB = '\t';
    constexpr char CR = '\r';
    constexpr char LF = '\n';

    constexpr std::string_view CRLF = "\r\n";
    constexpr std::string_view HeaderSectionEnd = "\r\n\r\n";
    constexpr std::string_view TokenSpecialChars = "!#$%&'*+-.^_`|~";
    constexpr char HeaderKeyEnd = ':';
    constexpr char ListSeparator = ',';
    constexpr char ChunkExtSeparator = ';';
    constexpr char QuerySeparator = '?';
    constexpr char PathPrefix = '/';

    namespace Host {
        constexpr std::string_view NameSymbols = "-._~!$&'()*+,;=%";
        constexpr std::string_view ConsecutiveDots = "..";
        constexpr char PortSeparator = ':';
        constexpr char LiteralOpen = '[';
        constexpr char LiteralClose = ']';
    } // namespace Host

} // namespace Http::Syntax

namespace Http::Protocol {
    constexpr std::string_view Name = "HTTP";
    constexpr std::string_view VersionPrefix = "HTTP/";
    constexpr char VersionSeparator = '/';
    constexpr char VersionComponentSeparator = '.';
    constexpr std::size_t VersionComponentLength = 1;
    constexpr std::size_t VersionNumberLength = 3;
    constexpr std::string_view Version = "1.1";
} // namespace Http::Protocol
