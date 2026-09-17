#include "sha256.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace lit::crypto {
std::optional<std::vector<std::uint8_t>> hash(const std::string& password,
                                              const std::vector<std::uint8_t>& salt, int iterations,
                                              int hash_length) {
    std::vector<std::uint8_t> hash(static_cast<std::size_t>(hash_length));
    if (!PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()), salt.data(),
                           static_cast<int>(salt.size()), iterations, EVP_sha256(), hash_length,
                           hash.data())) {
        return std::nullopt;
    }
    return hash;
}

bool verify_hash(const std::string& password, const std::vector<std::uint8_t>& expected_hash,
                 const std::vector<std::uint8_t>& salt) {
    auto computed_hash = hash(password, salt);
    if (!computed_hash) return false;

    if (CRYPTO_memcmp(static_cast<const void*>(computed_hash.value().data()),
                      static_cast<const void*>(expected_hash.data()),
                      computed_hash.value().size()) != 0)
        return false;

    return true;
}

std::optional<std::vector<std::uint8_t>> generate_salt(std::size_t size) {
    std::vector<std::uint8_t> salt(size);
    if (RAND_bytes(salt.data(), static_cast<int>(size)) == -1) return std::nullopt;
    return salt;
}
}  // namespace lit::crypto
