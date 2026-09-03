#include <algorithm>

#include "HttpHeadersUtils.hpp"
#include "HttpSyntax.hpp"

namespace Http::Headers {
    bool isValidName(const std::string& name) {
        return !name.empty() && std::ranges::all_of(name, [](char c) {
            const unsigned char uc = static_cast<unsigned char>(c);

            return uc > 32 && uc < 127 && std::string_view(":()<>@,;\\\"/[]?={}").find(c) == std::string_view::npos;
        });
    }

    bool isValidValue(const std::string& value) {
        std::size_t index = 0;

        while (index < value.size()) {
            const char c = value[index];
            const unsigned char uc = static_cast<unsigned char>(c);

            if ((uc < 32 && c != Http::Syntax::HTAB) || uc == 127) {
                return false;
            }

            ++index;
        }

        return true;
    }
} // namespace Http::Headers
