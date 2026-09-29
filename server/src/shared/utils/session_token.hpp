#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace lit {
// A session token's SHA-256 — what the server keeps instead of the token, so
// that its memory (and later its save files) holds nothing to log in with.
using TokenHash = std::array<std::uint8_t, 32>;

// A new session token: 128 random bits from OpenSSL's CSPRNG as 32 lowercase
// hex digits. Throws std::runtime_error if the generator fails.
std::string new_session_token();

// The SHA-256 of `token`.
TokenHash hash_token(std::string_view token);
}  // namespace lit
