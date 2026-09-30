#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "game/v1/protocol.pb.h"
#include "state/world_state.hpp"
#include "systems/join_system.hpp"
#include "systems/presence_system.hpp"

namespace {

::game::v1::Hello hello(std::uint32_t version, std::string name) {
    ::game::v1::Hello h;
    h.set_protocol_version(version);
    h.set_name(std::move(name));
    return h;
}

constexpr auto kCurrent = ::game::v1::PROTOCOL_VERSION_CURRENT;

// U+202E as UTF-8 bytes: GCC refuses bidi controls in source, even escaped.
const std::string kRightToLeftOverride = "\xE2\x80\xAE";

std::optional<std::string> name(std::string_view raw) { return lit::game::normalize_name(raw); }

}  // namespace

TEST(HelloRules, AcceptsTheCurrentVersionWithAName) {
    const auto joined = lit::game::check_hello(hello(kCurrent, "  tag "));

    ASSERT_TRUE(joined.has_value());
    EXPECT_EQ(*joined, "tag");
}

TEST(HelloRules, RejectsAnotherProtocolVersion) {
    const auto current = static_cast<std::uint32_t>(kCurrent);
    for (std::uint32_t version : {0U, current - 1, current + 1}) {  // unset, an old tab, a newer
        const auto joined = lit::game::check_hello(hello(version, "tag"));
        ASSERT_FALSE(joined.has_value()) << version;
        EXPECT_EQ(joined.error(), ::game::v1::ERROR_CODE_PROTOCOL_VERSION);
    }
}

TEST(HelloRules, RejectsABadName) {
    const auto joined = lit::game::check_hello(hello(kCurrent, "   "));

    ASSERT_FALSE(joined.has_value());
    EXPECT_EQ(joined.error(), ::game::v1::ERROR_CODE_INVALID_NAME);
}

TEST(PlayerName, TrimsSurroundingSpaces) {
    EXPECT_EQ(name("  tag\t"), "tag");
    EXPECT_EQ(name("\u00A0tag\u3000"), "tag");  // no-break and ideographic spaces too
    EXPECT_EQ(name("Big Bob"), "Big Bob");      // inner spaces stay
}

TEST(PlayerName, RejectsEmptyOrBlank) {
    EXPECT_FALSE(name(""));
    EXPECT_FALSE(name("   "));
    EXPECT_FALSE(name("\u00A0"));
}

TEST(PlayerName, CountsCharactersNotBytes) {
    EXPECT_EQ(name("абвгдеёжзийклмно"), "абвгдеёжзийклмно");  // 16 letters, 32 bytes
    EXPECT_FALSE(name("абвгдеёжзийклмноп"));                  // 17
    EXPECT_EQ(name("abcdefghijklmnop"), "abcdefghijklmnop");
    EXPECT_FALSE(name("abcdefghijklmnopq"));
    EXPECT_EQ(name("\U0001F600"), "\U0001F600");  // one character, four bytes
}

TEST(PlayerName, RejectsControlCharacters) {
    EXPECT_FALSE(name("a\tb"));
    EXPECT_FALSE(name("a\x7F"));
    EXPECT_FALSE(name("a\u0085b"));  // C1 "next line"
}

TEST(PlayerName, RejectsInvisibleCharacters) {
    EXPECT_FALSE(name("a\u200Bb"));  // zero-width space
    EXPECT_FALSE(name(kRightToLeftOverride + "evil"));
    EXPECT_FALSE(name("\uFEFFa"));   // byte order mark
    EXPECT_FALSE(name("a\u00ADb"));  // soft hyphen
}

TEST(PlayerName, IsTakenIgnoringCase) {
    lit::game::WorldState state;
    lit::game::create_character(state, "Tag", {});
    lit::game::create_character(state, "Ёжик", {});
    lit::game::create_character(state, "Ђура", {});

    EXPECT_TRUE(lit::game::is_name_taken(state, "Tag"));
    EXPECT_TRUE(lit::game::is_name_taken(state, "tag"));
    EXPECT_TRUE(lit::game::is_name_taken(state, "TAG"));
    EXPECT_TRUE(lit::game::is_name_taken(state, "ёЖИК"));  // Cyrillic, Ё included
    EXPECT_TRUE(lit::game::is_name_taken(state, "ЁЖИК"));
    EXPECT_TRUE(lit::game::is_name_taken(state, "ђУРА"));  // Ѐ–Џ fold too
    EXPECT_FALSE(lit::game::is_name_taken(state, "Tag2"));
    EXPECT_FALSE(lit::game::is_name_taken(state, "Ta"));
    EXPECT_FALSE(lit::game::is_name_taken(state, "Еж"));
}

TEST(PlayerName, StaysTakenAfterItsCharacterLeaves) {
    lit::game::WorldState state;
    const auto id = lit::game::create_character(state, "Tag", {}).id;

    lit::game::leave_world(state, id);

    EXPECT_TRUE(lit::game::is_name_taken(state, "tag"));  // the record keeps it
}

TEST(PlayerName, RejectsInvalidUtf8) {
    EXPECT_FALSE(name("a\xC3"));             // truncated
    EXPECT_FALSE(name("\xC0\xAF"));          // overlong '/'
    EXPECT_FALSE(name("\xED\xA0\x80"));      // a UTF-16 surrogate
    EXPECT_FALSE(name("\xF4\x90\x80\x80"));  // past U+10FFFF
    EXPECT_FALSE(name("\x80"));              // a stray continuation byte
}
