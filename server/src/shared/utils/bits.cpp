#include "utils/bits.hpp"

namespace lit {
std::string pack_bits(const std::vector<std::uint8_t>& flags) {
    std::string bits(packed_size(flags.size()), '\0');
    for (std::size_t i = 0; i < flags.size(); ++i) {
        if (flags[i] == 0) continue;
        const auto byte = static_cast<unsigned>(static_cast<unsigned char>(bits[i / 8]));
        bits[i / 8] = static_cast<char>(byte | (1U << (i % 8)));
    }
    return bits;
}

std::vector<std::uint8_t> unpack_bits(std::string_view bits, std::size_t count) {
    std::vector<std::uint8_t> flags(count, 0);
    for (std::size_t i = 0; i < count && i / 8 < bits.size(); ++i) {
        const auto byte = static_cast<unsigned>(static_cast<unsigned char>(bits[i / 8]));
        flags[i] = static_cast<std::uint8_t>((byte >> (i % 8)) & 1U);
    }
    return flags;
}
}  // namespace lit
