#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ep::crypto {
enum class Crypto : std::uint32_t { kNumIterations = 10'000, kHashLength = 32, kSaltLength = 16 };

std::optional<std::vector<std::uint8_t>> hash(
    const std::string& password, const std::vector<std::uint8_t>& salt,
    int iterations = static_cast<int>(Crypto::kNumIterations),
    int hash_length = static_cast<int>(Crypto::kHashLength));

bool verify_hash(const std::string& password, const std::vector<std::uint8_t>& expected_hash,
                 const std::vector<std::uint8_t>& salt);

std::optional<std::vector<std::uint8_t>> generate_salt(
    std::size_t size = static_cast<std::size_t>(Crypto::kSaltLength));
}  // namespace ep::crypto
