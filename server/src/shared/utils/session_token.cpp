#include "utils/session_token.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace lit {
namespace {
constexpr std::size_t kTokenBytes = 16;  // 128 bits
}  // namespace

std::string new_session_token() {
    std::array<unsigned char, kTokenBytes> bytes{};
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        throw std::runtime_error("RAND_bytes failed: no session token");
    }
    constexpr std::string_view kDigits = "0123456789abcdef";
    std::string token;
    token.reserve(2 * kTokenBytes);
    for (unsigned char byte : bytes) {
        token += kDigits[byte >> 4U];
        token += kDigits[byte & 0xFU];
    }
    return token;
}

TokenHash hash_token(std::string_view token) {
    TokenHash hash{};
    unsigned int size = 0;
    if (EVP_Digest(token.data(), token.size(), hash.data(), &size, EVP_sha256(), nullptr) != 1 ||
        size != hash.size()) {
        throw std::runtime_error("EVP_Digest(SHA-256) failed");
    }
    return hash;
}
}  // namespace lit
