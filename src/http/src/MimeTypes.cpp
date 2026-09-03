#include "MimeTypes.hpp"

#include "HttpUtils.hpp"

#include <array>
#include <string_view>

namespace {
    struct MimeEntry {
        std::string_view extension;
        std::string_view type;
    };

    constexpr auto mimeTypes = std::to_array<MimeEntry>({
        {".html", Http::Mime::Html},
        {".htm", Http::Mime::Html},
        {".css", "text/css; charset=utf-8"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".txt", "text/plain; charset=utf-8"},
        {".xml", "application/xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".webp", "image/webp"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".pdf", "application/pdf"},
        {".wasm", "application/wasm"},
    });
} // namespace

namespace Http::Mime {
    std::string from(const std::filesystem::path& path) {
        const std::string extension = Http::Ascii::tolower(path.extension().string());

        const auto it = std::ranges::find(mimeTypes, extension, &MimeEntry::extension);

        return it != mimeTypes.end() ? std::string(it->type) : "application/octet-stream";
    }
} // namespace Http::Mime
