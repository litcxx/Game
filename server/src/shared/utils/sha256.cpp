#include "utils/sha256.hpp"

#include <openssl/evp.h>

#include <stdexcept>
#include <string_view>

namespace lit {
Sha256 sha256(std::string_view bytes) {
    Sha256 digest{};
    unsigned int size = 0;
    if (EVP_Digest(bytes.data(), bytes.size(), digest.data(), &size, EVP_sha256(), nullptr) != 1 ||
        size != digest.size()) {
        throw std::runtime_error("EVP_Digest(SHA-256) failed");
    }
    return digest;
}
}  // namespace lit
