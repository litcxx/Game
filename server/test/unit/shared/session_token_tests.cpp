#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

#include "utils/session_token.hpp"

namespace {

std::string hex(const lit::TokenHash& hash) {
    constexpr std::string_view kDigits = "0123456789abcdef";
    std::string out;
    for (std::uint8_t byte : hash) {
        out += kDigits[byte >> 4U];
        out += kDigits[byte & 0xFU];
    }
    return out;
}

}  // namespace

TEST(SessionToken, IsThirtyTwoLowercaseHexDigits) {
    const std::string token = lit::new_session_token();

    ASSERT_EQ(token.size(), 32u);  // 128 bits
    for (char c : token) EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) << token;
}

TEST(SessionToken, EveryTokenIsNew) {
    std::set<std::string> seen;
    for (int i = 0; i < 1000; ++i) seen.insert(lit::new_session_token());

    EXPECT_EQ(seen.size(), 1000u);
}

TEST(SessionToken, TheHashIsSha256) {
    // FIPS 180-2 test vectors.
    EXPECT_EQ(hex(lit::hash_token("abc")),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(hex(lit::hash_token("")),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(SessionToken, OneTokenOneHash) {
    const std::string token = lit::new_session_token();

    EXPECT_EQ(lit::hash_token(token), lit::hash_token(token));
    EXPECT_NE(lit::hash_token(token), lit::hash_token(lit::new_session_token()));
}
