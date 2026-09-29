#include <gtest/gtest.h>

#include <cstdint>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/vision.hpp"
#include "state/world_state.hpp"
#include "systems/vision_system.hpp"

namespace {

// A 10x10-cell map; sight reaches 2 cells (200 units) from a source to a cell
// centre. Cell (col, row) has its centre at (col * 100 + 50, row * 100 + 50).
struct Field {
    lit::GameConfig config = [] {
        lit::GameConfig c{};
        c.map_width = 10;
        c.map_height = 10;
        c.vision_radius = 200;
        return c;
    }();
    lit::game::WorldState state = [] {
        lit::game::WorldState s;
        s.territory.reset(10, 10);
        return s;
    }();

    // A unit of `faction` at (x, y).
    lit::game::Unit& add_unit(std::uint32_t id, std::uint32_t faction, double x, double y,
                              ::game::v1::LifeState life = ::game::v1::LIFE_STATE_ALIVE) {
        lit::game::Unit u;
        u.id = id;
        u.faction_id = faction;
        u.life = life;
        u.x = x;
        u.y = y;
        return state.units[id] = u;
    }

    void own(std::uint32_t col, std::uint32_t row, std::uint8_t faction) {
        state.territory.owners[row * 10 + col] = faction;
    }

    lit::game::Vision of(std::uint32_t faction) const {
        return lit::game::compute_vision(state, config, faction);
    }
};

bool sees(const lit::game::Vision& v, std::uint32_t col, std::uint32_t row) {
    return v.sees(row * 10 + col);
}

int count_visible(const lit::game::Vision& v) {
    int n = 0;
    for (std::uint32_t i = 0; i < 100; ++i) n += v.sees(i) ? 1 : 0;
    return n;
}

}  // namespace

TEST(Vision, AlivePlayerSeesCellsWithinTheRadius) {
    Field f;
    f.add_unit(1, 1, 550, 550);  // centre of cell (5,5)

    const auto v = f.of(1);

    EXPECT_TRUE(sees(v, 5, 5));       // its own cell
    EXPECT_TRUE(sees(v, 6, 6));       // 141 away
    EXPECT_TRUE(sees(v, 5, 3));       // 200 away
    EXPECT_FALSE(sees(v, 7, 6));      // 224 away
    EXPECT_FALSE(sees(v, 8, 5));      // 300 away
    EXPECT_EQ(count_visible(v), 13);  // a disc: every (dx, dy) with dx^2 + dy^2 <= 2^2 cells
}

TEST(Vision, TheRadiusIsInclusive) {
    Field f;
    f.add_unit(1, 1, 550, 550);

    const auto v = f.of(1);

    EXPECT_TRUE(sees(v, 7, 5));  // centre exactly 200 away
    EXPECT_TRUE(sees(v, 3, 5));
}

TEST(Vision, IsMeasuredFromThePlayersExactPosition) {
    Field f;
    f.add_unit(1, 1, 599, 550);  // near the right edge of cell (5,5)

    const auto v = f.of(1);

    EXPECT_TRUE(sees(v, 7, 5));   // centre 151 away
    EXPECT_FALSE(sees(v, 3, 5));  // centre 249 away
}

TEST(Vision, StopsAtTheMapEdge) {
    Field f;
    f.add_unit(1, 1, 50, 50);  // the corner cell

    const auto v = f.of(1);

    EXPECT_TRUE(sees(v, 0, 0));
    EXPECT_TRUE(sees(v, 2, 0));
    EXPECT_TRUE(sees(v, 1, 1));
    EXPECT_EQ(count_visible(v), 6);  // the quarter of the disc that is on the map
}

TEST(Vision, AlliesShareVision) {
    Field f;
    f.add_unit(1, 1, 150, 150);
    f.add_unit(2, 1, 850, 850);  // same faction, far away

    const auto v = f.of(1);

    EXPECT_TRUE(sees(v, 1, 1));
    EXPECT_TRUE(sees(v, 8, 8));
    EXPECT_FALSE(sees(v, 5, 5));  // between them, out of both ranges
}

TEST(Vision, EnemiesGiveNoVision) {
    Field f;
    f.add_unit(1, 2, 550, 550);

    EXPECT_EQ(count_visible(f.of(1)), 0);
    EXPECT_EQ(count_visible(f.of(2)), 13);
}

TEST(Vision, DeadPlayersGiveNoVision) {
    Field f;
    f.add_unit(1, 1, 550, 550, ::game::v1::LIFE_STATE_DEAD);

    EXPECT_EQ(count_visible(f.of(1)), 0);
}

TEST(Vision, ABodyKeepsItsSightUntilItsDeathIsReported) {
    Field f;
    f.add_unit(1, 1, 550, 550, ::game::v1::LIFE_STATE_DEAD);
    f.state.events.emplace_back().mutable_death()->set_victim_id(1);  // not yet in a snapshot

    EXPECT_EQ(count_visible(f.of(1)), 13);  // the snapshot reporting the death still sees
}

TEST(Vision, OwnedCellsGiveVision) {
    Field f;
    f.own(2, 2, 1);  // no players at all

    const auto v = f.of(1);

    EXPECT_TRUE(sees(v, 2, 2));
    EXPECT_TRUE(sees(v, 4, 2));            // 2 cells from the owned one
    EXPECT_FALSE(sees(v, 5, 2));           // 3 cells
    EXPECT_EQ(count_visible(f.of(2)), 0);  // another faction's cell shows nothing
}

TEST(Vision, NoFactionSeesNothing) {
    Field f;  // every cell is neutral (owner 0): still not a source for "no faction"
    f.add_unit(1, 1, 550, 550);

    EXPECT_EQ(count_visible(f.of(0)), 0);
}

TEST(Vision, DefaultVisionSeesNothing) {
    const lit::game::Vision none;

    EXPECT_FALSE(none.sees(0));
    EXPECT_FALSE(none.sees(99));
}
