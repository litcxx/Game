#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "utils/bits.hpp"

TEST(Bits, PackTheLowestBitFirst) {
    std::vector<std::uint8_t> flags(10, 0);
    flags[0] = 1;  // bit 0 of byte 0
    flags[9] = 1;  // bit 1 of byte 1

    EXPECT_EQ(lit::pack_bits(flags), std::string("\x01\x02", 2));
}

TEST(Bits, TakeAByteForEveryEightItems) {
    EXPECT_EQ(lit::pack_bits(std::vector<std::uint8_t>(16, 1)), std::string("\xFF\xFF", 2));
    EXPECT_EQ(lit::pack_bits(std::vector<std::uint8_t>(17, 0)).size(), 3u);
    EXPECT_TRUE(lit::pack_bits({}).empty());
    EXPECT_EQ(lit::packed_size(17), 3u);
    EXPECT_EQ(lit::packed_size(16), 2u);
}

TEST(Bits, AnyNonZeroFlagIsSet) { EXPECT_EQ(lit::pack_bits({7, 0, 255}), std::string("\x05", 1)); }

TEST(Bits, UnpackGivesTheFlagsBack) {
    std::mt19937 random{20261002};
    std::bernoulli_distribution coin;
    std::vector<std::uint8_t> flags(101);
    for (auto& flag : flags) flag = coin(random) ? 1 : 0;

    EXPECT_EQ(lit::unpack_bits(lit::pack_bits(flags), flags.size()), flags);
}

TEST(Bits, BitsPastTheEndReadAsZero) {
    const std::vector<std::uint8_t> expected{1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0};

    EXPECT_EQ(lit::unpack_bits("\xFF", 12), expected);
    EXPECT_EQ(lit::unpack_bits("", 3), (std::vector<std::uint8_t>{0, 0, 0}));
}
