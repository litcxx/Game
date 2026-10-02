#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>

#include "state/faction_memory.hpp"
#include "state/territory.hpp"
#include "state/vision.hpp"

namespace {

// A 4x4 map: Blue (2) holds cell 1, Red (1) cell 2.
lit::game::Territory map() {
    lit::game::Territory t;
    t.reset(4, 4);
    t.set_owner(1, 2);
    t.set_owner(2, 1);
    return t;
}

// The sight of a faction that sees exactly `cells` of a 4x4 map.
lit::game::Vision sees(std::initializer_list<std::uint32_t> cells) {
    lit::game::Vision v;
    v.cells.assign(16, 0);
    for (std::uint32_t index : cells) v.cells[index] = 1;
    return v;
}

}  // namespace

TEST(FactionMemory, RemembersTheCellsItSeesWithTheirOwners) {
    const auto t = map();
    lit::game::FactionMemory memory;

    memory.see(t, sees({0, 1, 2}));

    EXPECT_TRUE(memory.knows(0));
    EXPECT_TRUE(memory.knows(1));
    EXPECT_TRUE(memory.knows(2));
    EXPECT_FALSE(memory.knows(3));
    ASSERT_EQ(memory.owners.size(), 16u);
    EXPECT_EQ(memory.owners[0], 0);
    EXPECT_EQ(memory.owners[1], 2);
    EXPECT_EQ(memory.owners[2], 1);
    EXPECT_EQ(memory.owners[3], 0);
}

// A cell in the fog keeps the owner the faction last saw, whatever happens there.
TEST(FactionMemory, KeepsTheLastOwnerItSawUntilItSeesTheCellAgain) {
    auto t = map();
    lit::game::FactionMemory memory;
    memory.see(t, sees({1}));

    t.set_owner(1, 3);  // Green takes it, unseen
    memory.see(t, sees({5}));
    EXPECT_TRUE(memory.knows(1));
    EXPECT_EQ(memory.owners[1], 2);

    memory.see(t, sees({1}));  // seen again
    EXPECT_EQ(memory.owners[1], 3);
}

TEST(FactionMemory, WhatItSawBeforeStays) {
    const auto t = map();
    lit::game::FactionMemory memory;

    memory.see(t, sees({0}));
    memory.see(t, sees({15}));

    EXPECT_TRUE(memory.knows(0));
    EXPECT_TRUE(memory.knows(15));
}

// Faction 0 (none chosen) has an empty sight: it remembers nothing.
TEST(FactionMemory, SeeingNothingRemembersNothing) {
    const auto t = map();
    lit::game::FactionMemory memory;

    memory.see(t, lit::game::Vision{});

    for (std::uint32_t index = 0; index < 16; ++index) EXPECT_FALSE(memory.knows(index));
}
