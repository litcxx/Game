#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lit {
// A flag per item packed eight to a byte, the lowest bit first: item i is bit
// i % 8 of byte i / 8 (MapState.explored, the world save's faction memory).
std::string pack_bits(const std::vector<std::uint8_t>& flags);

// `count` flags (1 or 0) back from packed `bits`; bits past its end read as 0.
std::vector<std::uint8_t> unpack_bits(std::string_view bits, std::size_t count);

// The bytes `count` packed flags take.
constexpr std::size_t packed_size(std::size_t count) { return (count + 7) / 8; }
}  // namespace lit
