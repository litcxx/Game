#include "systems/join_system.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lit::game {
namespace {
constexpr std::size_t kMaxNameChars = 16;

// Decodes UTF-8 into code points; nullopt when malformed (truncated, overlong,
// a stray continuation byte, a UTF-16 surrogate or past U+10FFFF).
std::optional<std::vector<char32_t>> decode_utf8(std::string_view text) {
    std::vector<char32_t> out;
    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<unsigned char>(text[i]);
        std::size_t len = 0;
        char32_t cp = 0;
        char32_t min = 0;  // the smallest code point this length may encode
        if (lead < 0x80) {
            len = 1;
            cp = lead;
        } else if ((lead & 0xE0U) == 0xC0) {
            len = 2;
            cp = lead & 0x1FU;
            min = 0x80;
        } else if ((lead & 0xF0U) == 0xE0) {
            len = 3;
            cp = lead & 0x0FU;
            min = 0x800;
        } else if ((lead & 0xF8U) == 0xF0) {
            len = 4;
            cp = lead & 0x07U;
            min = 0x10000;
        } else {
            return std::nullopt;  // a continuation byte or an invalid lead
        }
        if (i + len > text.size()) return std::nullopt;
        for (std::size_t k = 1; k < len; ++k) {
            const auto next = static_cast<unsigned char>(text[i + k]);
            if ((next & 0xC0U) != 0x80) return std::nullopt;
            cp = (cp << 6U) | (next & 0x3FU);
        }
        if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return std::nullopt;
        out.push_back(cp);
        i += len;
    }
    return out;
}

// Bytes a code point takes in UTF-8.
std::size_t utf8_length(char32_t cp) {
    if (cp < 0x80) return 1;
    if (cp < 0x800) return 2;
    if (cp < 0x10000) return 3;
    return 4;
}

// Space separators: trimmed off the ends of a name, allowed inside it.
bool is_space(char32_t cp) {
    return cp == 0x20 || cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) ||
           cp == 0x202F || cp == 0x205F || cp == 0x3000 || (cp >= 0x09 && cp <= 0x0D);
}

// Characters a name must not contain: controls (C0, DEL, C1) and the invisible
// ones — soft hyphen, zero-width and joiner characters, direction marks,
// embeddings, overrides and isolates, line and paragraph separators, the byte
// order mark and noncharacters.
bool is_forbidden(char32_t cp) {
    return cp < 0x20 || (cp >= 0x7F && cp <= 0x9F) || cp == 0xAD ||
           (cp >= 0x200B && cp <= 0x200F) || (cp >= 0x2028 && cp <= 0x202E) ||
           (cp >= 0x2060 && cp <= 0x206F) || cp == 0xFEFF || (cp >= 0xFFF9 && cp <= 0xFFFB) ||
           (cp & 0xFFFEU) == 0xFFFE;
}

// The lowercase of a Latin or Cyrillic capital; any other code point as is.
char32_t fold_case(char32_t cp) {
    if (cp >= U'A' && cp <= U'Z') return cp + 0x20;
    if (cp >= 0x0410 && cp <= 0x042F) return cp + 0x20;  // А–Я -> а–я
    if (cp >= 0x0400 && cp <= 0x040F) return cp + 0x50;  // Ѐ–Џ (Ё among them) -> ѐ–џ
    return cp;
}

// A name's code points with the case folded: names match when their keys do.
std::vector<char32_t> name_key(std::string_view name) {
    std::vector<char32_t> key = decode_utf8(name).value_or(std::vector<char32_t>{});
    std::ranges::transform(key, key.begin(), fold_case);
    return key;
}
}  // namespace

std::expected<std::string, ::game::v1::ErrorCode> check_hello(const ::game::v1::Hello& hello) {
    if (hello.protocol_version() != ::game::v1::PROTOCOL_VERSION_CURRENT) {
        return std::unexpected(::game::v1::ERROR_CODE_PROTOCOL_VERSION);
    }
    auto name = normalize_name(hello.name());
    if (!name) return std::unexpected(::game::v1::ERROR_CODE_INVALID_NAME);
    return *std::move(name);
}

std::optional<std::string> normalize_name(std::string_view raw) {
    const auto chars = decode_utf8(raw);
    if (!chars) return std::nullopt;

    // Trim spaces off both ends (in characters).
    std::size_t first = 0;
    std::size_t last = chars->size();
    while (first < last && is_space((*chars)[first])) ++first;
    while (last > first && is_space((*chars)[last - 1])) --last;
    const std::size_t count = last - first;
    if (count == 0 || count > kMaxNameChars) return std::nullopt;
    if (std::any_of(chars->begin() + static_cast<std::ptrdiff_t>(first),
                    chars->begin() + static_cast<std::ptrdiff_t>(last), is_forbidden)) {
        return std::nullopt;
    }
    // Re-slice the original bytes: re-encoding is not needed for valid UTF-8.
    std::size_t begin = 0;
    for (std::size_t k = 0; k < first; ++k) begin += utf8_length((*chars)[k]);
    std::size_t length = 0;
    for (std::size_t k = first; k < last; ++k) length += utf8_length((*chars)[k]);
    return std::string{raw.substr(begin, length)};
}

bool is_name_taken(const WorldState& state, std::string_view name) {
    const std::vector<char32_t> key = name_key(name);
    return std::ranges::any_of(
        state.characters, [&key](const auto& entry) { return name_key(entry.second.name) == key; });
}
}  // namespace lit::game
