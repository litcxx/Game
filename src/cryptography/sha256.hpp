#pragma once

#include <optional>
#include <vector>
#include <cstdint>
#include <string>

namespace ep::crypto
{
  enum class Crypto : std::uint32_t {
    kNumIterations = 10'000,
    kHashLength = 32,
    kSaltLength = 16
  };

  std::optional<std::vector<std::uint8_t>> Hash(const std::string& password, 
                                         const std::vector<std::uint8_t>& salt,
                                         int iterations = static_cast<int>(Crypto::kNumIterations),
                                         int hash_length = static_cast<int>(Crypto::kHashLength));
 
  bool VerifyHash(const std::string& password, 
                  const std::vector<std::uint8_t>& expected_hash, 
                  const std::vector<std::uint8_t>& salt);

  std::optional<std::vector<std::uint8_t>> GenerateSalt(std::size_t size = static_cast<std::size_t>(Crypto::kSaltLength));
}
