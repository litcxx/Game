#include <gtest/gtest.h>

#include "state/territory.hpp"

namespace {

// A 4x4-cell map: 16 cells, all neutral.
lit::game::Territory fresh() {
    lit::game::Territory t;
    t.reset(4, 4);
    return t;
}

}  // namespace

TEST(TerritoryCounts, AFreshMapIsAllNeutral) {
    const auto t = fresh();

    EXPECT_EQ(t.owned(0), 16u);
    EXPECT_EQ(t.owned(1), 0u);
}

TEST(TerritoryCounts, SettingAnOwnerMovesTheCellBetweenCounts) {
    auto t = fresh();

    t.set_owner(0, 1);
    t.set_owner(1, 1);
    t.set_owner(2, 2);
    EXPECT_EQ(t.owned(1), 2u);
    EXPECT_EQ(t.owned(2), 1u);
    EXPECT_EQ(t.owned(0), 13u);

    t.set_owner(1, 2);  // taken over
    EXPECT_EQ(t.owned(1), 1u);
    EXPECT_EQ(t.owned(2), 2u);
    EXPECT_EQ(t.owners[1], 2);
}

TEST(TerritoryCounts, TheSameOwnerAgainChangesNothing) {
    auto t = fresh();
    t.set_owner(5, 3);

    t.set_owner(5, 3);

    EXPECT_EQ(t.owned(3), 1u);
    EXPECT_EQ(t.owned(0), 15u);
}

TEST(TerritoryCounts, AResetStartsOver) {
    auto t = fresh();
    t.set_owner(5, 3);

    t.reset(2, 2);

    EXPECT_EQ(t.owned(3), 0u);
    EXPECT_EQ(t.owned(0), 4u);
}
