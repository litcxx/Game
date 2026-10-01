#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/territory.hpp"
#include "state/unit.hpp"
#include "state/world_state.hpp"
#include "systems/capital_system.hpp"
#include "systems/capture_system.hpp"

namespace {

// A 5x5-cell map; cell (col, row) has its centre at (col * 100 + 50, row * 100 + 50).
// The shipped capture times: a neutral cell in 90 ticks, an enemy one twice as long.
struct Front {
    lit::GameConfig config = [] {
        lit::GameConfig c{};
        c.map_width = 5;
        c.map_height = 5;
        c.capture_ticks = 90;
        c.capture_enemy_multiplier = 2.0;
        return c;
    }();
    lit::game::WorldState state = [] {
        lit::game::WorldState s;
        s.territory.reset(5, 5);
        return s;
    }();

    static std::uint32_t cell(std::uint32_t col, std::uint32_t row) { return row * 5 + col; }

    void own(std::uint32_t col, std::uint32_t row, std::uint8_t faction) {
        state.territory.owners[cell(col, row)] = faction;
    }
    void protect(std::uint32_t col, std::uint32_t row) {
        state.territory.protected_cells[cell(col, row)] = true;
    }
    std::uint8_t owner(std::uint32_t col, std::uint32_t row) const {
        return state.territory.owners[cell(col, row)];
    }
    double progress(std::uint32_t col, std::uint32_t row) const {
        return state.territory.capture_progress[cell(col, row)];
    }

    bool can(std::uint32_t col, std::uint32_t row, std::uint32_t faction) const {
        return lit::game::capturable(state.territory, cell(col, row), faction);
    }

    // A unit of `faction` holding capture at the centre of (col, row).
    void capturer(std::uint32_t id, std::uint32_t faction, std::uint32_t col, std::uint32_t row) {
        lit::game::Unit u;
        u.id = id;
        u.faction_id = faction;
        u.life = ::game::v1::LIFE_STATE_ALIVE;
        u.x = col * 100.0 + 50.0;
        u.y = row * 100.0 + 50.0;
        u.intent.capturing = true;
        state.units[id] = u;
    }

    // Ticks until (col, row) turns `faction`'s, up to `limit`; 0 = not by then.
    int ticks_to_capture(std::uint32_t col, std::uint32_t row, std::uint8_t faction,
                         int limit = 1000) {
        for (int tick = 1; tick <= limit; ++tick) {
            lit::game::update_captures(state, config);
            if (owner(col, row) == faction) return tick;
        }
        return 0;
    }
};

}  // namespace

// --- Which cells: next to ours by a side, not a protected one (GDD 7.3) ----------

TEST(Capturable, ACellNextToOursByASide) {
    Front f;
    f.own(2, 2, 1);

    EXPECT_TRUE(f.can(1, 2, 1));
    EXPECT_TRUE(f.can(3, 2, 1));
    EXPECT_TRUE(f.can(2, 1, 1));
    EXPECT_TRUE(f.can(2, 3, 1));
}

TEST(Capturable, NotACellAwayFromOurs) {
    Front f;
    f.own(2, 2, 1);

    EXPECT_FALSE(f.can(4, 2, 1));  // two cells away
    EXPECT_FALSE(f.can(0, 0, 1));
}

TEST(Capturable, NotACornerNeighbour) {
    Front f;
    f.own(2, 2, 1);

    EXPECT_FALSE(f.can(1, 1, 1));
    EXPECT_FALSE(f.can(3, 3, 1));
}

TEST(Capturable, NotAnythingWithoutACellOfOurs) {
    Front f;
    f.own(2, 2, 1);

    EXPECT_FALSE(f.can(1, 2, 2));  // Blue holds nothing to grow from
}

TEST(Capturable, NotOurOwnCell) {
    Front f;
    f.own(2, 2, 1);
    f.own(3, 2, 1);

    EXPECT_FALSE(f.can(2, 2, 1));
}

TEST(Capturable, AnEnemyCellNextToOurs) {
    Front f;
    f.own(2, 2, 1);
    f.own(3, 2, 2);

    EXPECT_TRUE(f.can(3, 2, 1));
    EXPECT_TRUE(f.can(2, 2, 2));  // and the other way round: a front
}

// A row's last cell and the next row's first are neighbours by index, not on the map.
TEST(Capturable, NotAcrossTheMapEdge) {
    Front f;
    f.own(4, 1, 1);  // Red: on the right edge
    f.own(0, 3, 2);  // Blue: on the left edge

    EXPECT_FALSE(f.can(0, 2, 1));  // index of (4, 1) + 1
    EXPECT_FALSE(f.can(4, 2, 2));  // index of (0, 3) - 1
}

TEST(Capturable, NotAProtectedCell) {
    Front f;
    f.own(2, 2, 1);
    f.own(3, 2, 2);
    f.protect(3, 2);  // in Blue's capital zone

    EXPECT_FALSE(f.can(3, 2, 1));
    EXPECT_TRUE(f.can(2, 2, 2));  // the zone's own faction still pushes out of it
}

// --- How long: capture_ticks, an enemy cell × capture_enemy_multiplier -----------

TEST(UpdateCaptures, ANeutralCellTakesCaptureTicks) {
    Front f;
    f.own(2, 2, 1);
    f.capturer(1, 1, 3, 2);

    EXPECT_EQ(f.ticks_to_capture(3, 2, 1), 90);
}

TEST(UpdateCaptures, AnEnemyCellTakesTheMultiplierLonger) {
    Front f;
    f.own(2, 2, 1);
    f.own(3, 2, 2);
    f.capturer(1, 1, 3, 2);

    EXPECT_EQ(f.ticks_to_capture(3, 2, 1), 180);
}

// The progress adds up to exactly 100 % on the last tick whatever the step's
// rounding (100 / 120 does not sum to 100.0 in 120 doubles).
TEST(UpdateCaptures, TheTimeIsExactForAnyMultiplier) {
    Front f;
    f.config.capture_ticks = 80;
    f.config.capture_enemy_multiplier = 1.5;
    f.own(2, 2, 1);
    f.own(3, 2, 2);
    f.capturer(1, 1, 3, 2);

    EXPECT_EQ(f.ticks_to_capture(3, 2, 1), 120);
}

TEST(UpdateCaptures, ACellAwayFromOursIsNotCaptured) {
    Front f;
    f.own(2, 2, 1);
    f.capturer(1, 1, 4, 2);

    EXPECT_EQ(f.ticks_to_capture(4, 2, 1, 300), 0);
    EXPECT_EQ(f.progress(4, 2), 0.0);
    EXPECT_TRUE(f.state.territory.dirty.empty());  // nothing to tell anyone
}

TEST(UpdateCaptures, AProtectedCellIsNotCaptured) {
    Front f;
    f.own(2, 2, 1);
    f.own(3, 2, 2);
    f.protect(3, 2);
    f.capturer(1, 1, 3, 2);

    EXPECT_EQ(f.ticks_to_capture(3, 2, 1, 600), 0);
    EXPECT_EQ(f.owner(3, 2), 2);
    EXPECT_EQ(f.progress(3, 2), 0.0);
}

TEST(UpdateCaptures, LosingTheLinkResetsTheCapture) {
    Front f;
    f.own(2, 2, 1);
    f.capturer(1, 1, 3, 2);
    for (int i = 0; i < 45; ++i) lit::game::update_captures(f.state, f.config);
    ASSERT_GT(f.progress(3, 2), 0.0);
    f.state.territory.dirty.clear();

    f.own(2, 2, 2);  // the cell it grew from falls to Blue
    lit::game::update_captures(f.state, f.config);

    EXPECT_EQ(f.progress(3, 2), 0.0);
    EXPECT_EQ(f.state.territory.capture_faction[Front::cell(3, 2)], 0);
    EXPECT_TRUE(f.state.territory.dirty.contains(Front::cell(3, 2)));  // told: it stopped
    EXPECT_EQ(f.ticks_to_capture(3, 2, 1, 300), 0);
}

// --- The counts per faction (GAME-018) --------------------------------------------

// Taken cells change the owner counts as they go: after a long random fight over
// a map they still equal a recount from scratch.
TEST(UpdateCaptures, TheCountsMatchARecountAfterRandomCaptures) {
    Front f;
    f.config.capture_ticks = 3;
    f.state.territory.reset(12, 12);
    lit::game::seed_capitals(f.state.territory, {{1, 13, 1}, {2, 22, 1}, {3, 125, 1}});
    std::mt19937 random{20261001};
    std::uniform_int_distribution<std::uint32_t> pos{0, 1199};
    std::uniform_int_distribution<std::uint32_t> faction{1, 3};
    for (std::uint32_t id = 1; id <= 12; ++id) f.capturer(id, faction(random), 0, 0);

    std::uint32_t captured = 0;
    for (int tick = 0; tick < 6000; ++tick) {
        if (tick % 4 == 0) {  // everyone moves on now and then
            for (auto& [id, unit] : f.state.units) {
                unit.x = pos(random);
                unit.y = pos(random);
            }
        }
        captured += lit::game::update_captures(f.state, f.config);
    }

    ASSERT_GT(captured, 100u);  // a real fight: cells changed hands many times
    std::vector<std::uint32_t> recount(4, 0);
    for (std::uint8_t owner : f.state.territory.owners) ++recount[owner];
    for (std::uint32_t id = 0; id <= 3; ++id) {
        EXPECT_EQ(f.state.territory.owned(id), recount[id]) << "faction " << id;
    }
}
