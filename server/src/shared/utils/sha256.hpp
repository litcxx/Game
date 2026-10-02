#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace lit {
using Sha256 = std::array<std::uint8_t, 32>;

// The SHA-256 of `bytes` (OpenSSL). Throws std::runtime_error if it fails.
Sha256 sha256(std::string_view bytes);
}  // namespace lit
