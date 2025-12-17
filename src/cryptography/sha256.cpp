#include "sha256.hpp"

#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>

namespace ep::crypto
{
  std::optional<std::vector<std::uint8_t>> Hash(const std::string& password, 
                                         const std::vector<std::uint8_t>& salt,
                                         int iterations,
                                         int hash_length)
  {
    std::vector<std::uint8_t> hash(hash_length);
    if (!PKCS5_PBKDF2_HMAC(password.c_str(), 
                           static_cast<int>(password.size()),
                           salt.data(),
                           static_cast<int>(salt.size()),
                           iterations,
                           EVP_sha256(),
                           hash_length,
                           hash.data())) {
      return std::nullopt;
    }
    return hash;
  }

  bool VerifyHash(const std::string& password, 
                  const std::vector<std::uint8_t>& expected_hash, 
                  const std::vector<std::uint8_t>& salt)
  {
    auto hash = Hash(password, salt);
    if (!hash)
      return false;

    if (CRYPTO_memcmp(static_cast<const void*>(hash.value().data()), 
                      static_cast<const void*>(expected_hash.data()), 
                      hash.value().size()) != 0)
      return false;

    return true;
  }

  std::optional<std::vector<std::uint8_t>> GenerateSalt(std::size_t size)
  {
    std::vector<std::uint8_t> salt(size);
    if (RAND_bytes(salt.data(), size) == -1)
      return std::nullopt;
    return salt;
  }
}
