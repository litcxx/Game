#include <gtest/gtest.h>

#include <cstdint>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/unit.hpp"
#include "state/world_state.hpp"
#include "systems/presence_system.hpp"
#include "systems/spawn_system.hpp"

namespace {

// A character on a 4x4-cell map with two factions; not spawned yet.
struct Recruit {
    lit::GameConfig config = [] {
        lit::GameConfig c{};
        c.map_width = 4;
        c.map_height = 4;
        c.max_hp = 100;
        c.factions = {{1, "Red", 0xFF0000}, {2, "Blue", 0x0000FF}};
        return c;
    }();
    lit::game::WorldState state;
    std::uint32_t id = lit::game::create_character(state, "Ann", {}).id;

    auto spawn(std::uint32_t cell, std::uint32_t faction) {
        return lit::game::try_spawn(state, config, id, cell, faction);
    }
    lit::game::Unit& body() { return state.units.at(id); }
    std::uint32_t locked() const { return state.characters.at(id).faction_id; }

    // Its body dies, its respawn delay already over.
    void die() {
        body().hp = 0;
        body().life = ::game::v1::LIFE_STATE_DEAD;
        body().respawn_tick = state.tick;
    }
};

}  // namespace

TEST(SpawnFaction, TheFirstSpawnLocksTheFaction) {
    Recruit r;
    EXPECT_EQ(r.locked(), 0u);  // not chosen yet

    ASSERT_TRUE(r.spawn(5, 2));

    EXPECT_EQ(r.locked(), 2u);
    EXPECT_EQ(r.body().faction_id, 2u);
}

TEST(SpawnFaction, RespawningInAnotherFactionIsRefused) {
    Recruit r;
    ASSERT_TRUE(r.spawn(5, 1));
    r.die();

    const auto refused = r.spawn(6, 2);

    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error(), ::game::v1::ERROR_CODE_INVALID_FACTION);
    EXPECT_EQ(r.locked(), 1u);
    EXPECT_EQ(r.body().life, ::game::v1::LIFE_STATE_DEAD);  // the body stays where it fell
    EXPECT_EQ(r.body().faction_id, 1u);
}

TEST(SpawnFaction, RespawningInTheLockedFactionWorks) {
    Recruit r;
    ASSERT_TRUE(r.spawn(5, 1));
    r.die();

    ASSERT_TRUE(r.spawn(6, 1));

    EXPECT_EQ(r.body().life, ::game::v1::LIFE_STATE_ALIVE);
    EXPECT_EQ(r.locked(), 1u);
}

TEST(SpawnFaction, ARefusedFirstSpawnLocksNothing) {
    Recruit r;
    EXPECT_EQ(r.spawn(99, 2).error(), ::game::v1::ERROR_CODE_SPAWN_INVALID_CELL);
    EXPECT_EQ(r.spawn(5, 9).error(), ::game::v1::ERROR_CODE_INVALID_FACTION);  // no such faction
    EXPECT_EQ(r.locked(), 0u);

    ASSERT_TRUE(r.spawn(5, 1));  // still free to choose
    EXPECT_EQ(r.locked(), 1u);
}

TEST(SpawnFaction, TheLockOutlivesTheBody) {
    Recruit r;
    ASSERT_TRUE(r.spawn(5, 2));

    lit::game::leave_world(r.state, r.id);  // its grace is over: the body goes

    EXPECT_EQ(lit::game::find_unit(r.state, r.id), nullptr);
    EXPECT_EQ(lit::game::faction_of(r.state, r.id), 2u);  // the roster still shows it
}
