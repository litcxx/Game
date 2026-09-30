#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/unit.hpp"
#include "state/world_state.hpp"
#include "systems/presence_system.hpp"
#include "systems/spawn_system.hpp"

namespace {

// A character on a 4x4-cell map: Red's capital in cell 5 (col 1, row 1), Blue's
// in cell 10 (col 2, row 2), Green without one; not spawned yet. It spawns by the
// capital rule.
struct Recruit {
    lit::GameConfig config = [] {
        lit::GameConfig c{};
        c.map_width = 4;
        c.map_height = 4;
        c.max_hp = 100;
        c.factions = {{1, "Red", 0xFF0000}, {2, "Blue", 0x0000FF}, {3, "Green", 0x00FF00}};
        c.capitals = {{1, 5, 0}, {2, 10, 0}};
        return c;
    }();
    lit::game::WorldState state;
    std::uint32_t id = lit::game::create_character(state, "Ann", {}).id;

    auto spawn(std::uint32_t faction) {
        return lit::game::try_spawn(state, config, id, faction,
                                    lit::game::capital_spawn_point(config));
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

// --- Where: the faction's capital (GAME-016) ------------------------------------

TEST(CapitalSpawnPoint, IsTheCapitalCellOfTheFaction) {
    const Recruit r;
    const auto point = lit::game::capital_spawn_point(r.config);

    EXPECT_EQ(point(r.state, r.id, 1), std::optional<std::uint32_t>{5});
    EXPECT_EQ(point(r.state, r.id, 2), std::optional<std::uint32_t>{10});
    EXPECT_EQ(point(r.state, r.id, 3), std::nullopt);  // no capital: nowhere to spawn
}

TEST(SpawnAtCapital, TheBodyStandsAtTheCentreOfItsCapitalCell) {
    Recruit r;
    ASSERT_TRUE(r.spawn(2));

    EXPECT_EQ(r.body().x, 250.0);  // col 2
    EXPECT_EQ(r.body().y, 250.0);  // row 2
    EXPECT_EQ(r.body().life, ::game::v1::LIFE_STATE_ALIVE);
    EXPECT_EQ(r.body().hp, 100u);
}

TEST(SpawnAtCapital, ARespawnIsAtTheCapitalWhereverTheBodyFell) {
    Recruit r;
    ASSERT_TRUE(r.spawn(1));
    r.body().x = 350.0;  // it walked off and fell far from home
    r.body().y = 350.0;
    r.die();

    ASSERT_TRUE(r.spawn(1));

    EXPECT_EQ(r.body().x, 150.0);
    EXPECT_EQ(r.body().y, 150.0);
}

TEST(SpawnAtCapital, AFactionWithoutACapitalCannotSpawn) {
    Recruit r;
    EXPECT_EQ(r.spawn(3).error(), ::game::v1::ERROR_CODE_INVALID_FACTION);
    EXPECT_EQ(lit::game::find_unit(r.state, r.id), nullptr);
}

// The spawn point is asked only for a spawn that happens: a refused one never
// reaches it (a placement that hands out cells in turn relies on it).
TEST(SpawnAtCapital, TheSpawnPointIsAskedOnlyForASpawnThatHappens) {
    Recruit r;
    int asked = 0;
    const lit::game::SpawnPoint counting = [&asked](const lit::game::WorldState&, std::uint32_t,
                                                    std::uint32_t) -> std::optional<std::uint32_t> {
        ++asked;
        return 5;
    };
    const auto spawn = [&r, &counting](std::uint32_t faction) {
        return lit::game::try_spawn(r.state, r.config, r.id, faction, counting);
    };

    EXPECT_FALSE(spawn(9));  // no such faction
    EXPECT_TRUE(spawn(1));
    EXPECT_FALSE(spawn(1));  // alive
    r.die();
    r.body().respawn_tick = r.state.tick + 1;
    EXPECT_FALSE(spawn(1));  // too early
    r.body().respawn_tick = r.state.tick;
    EXPECT_FALSE(spawn(2));  // locked to Red

    EXPECT_EQ(asked, 1);
}

// --- Which: the faction is locked at the first spawn (GAME-014) -----------------

TEST(SpawnFaction, TheFirstSpawnLocksTheFaction) {
    Recruit r;
    EXPECT_EQ(r.locked(), 0u);  // not chosen yet

    ASSERT_TRUE(r.spawn(2));

    EXPECT_EQ(r.locked(), 2u);
    EXPECT_EQ(r.body().faction_id, 2u);
}

TEST(SpawnFaction, RespawningInAnotherFactionIsRefused) {
    Recruit r;
    ASSERT_TRUE(r.spawn(1));
    r.die();

    const auto refused = r.spawn(2);

    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error(), ::game::v1::ERROR_CODE_INVALID_FACTION);
    EXPECT_EQ(r.locked(), 1u);
    EXPECT_EQ(r.body().life, ::game::v1::LIFE_STATE_DEAD);  // the body stays where it fell
    EXPECT_EQ(r.body().faction_id, 1u);
}

TEST(SpawnFaction, RespawningInTheLockedFactionWorks) {
    Recruit r;
    ASSERT_TRUE(r.spawn(1));
    r.die();

    ASSERT_TRUE(r.spawn(1));

    EXPECT_EQ(r.body().life, ::game::v1::LIFE_STATE_ALIVE);
    EXPECT_EQ(r.locked(), 1u);
}

TEST(SpawnFaction, ARefusedFirstSpawnLocksNothing) {
    Recruit r;
    EXPECT_EQ(r.spawn(9).error(), ::game::v1::ERROR_CODE_INVALID_FACTION);  // no such faction
    EXPECT_EQ(r.spawn(3).error(), ::game::v1::ERROR_CODE_INVALID_FACTION);  // nowhere to spawn
    EXPECT_EQ(r.locked(), 0u);

    ASSERT_TRUE(r.spawn(1));  // still free to choose
    EXPECT_EQ(r.locked(), 1u);
}

TEST(SpawnFaction, TheLockOutlivesTheBody) {
    Recruit r;
    ASSERT_TRUE(r.spawn(2));

    lit::game::leave_world(r.state, r.id);  // its grace is over: the body goes

    EXPECT_EQ(lit::game::find_unit(r.state, r.id), nullptr);
    EXPECT_EQ(lit::game::faction_of(r.state, r.id), 2u);  // the roster still shows it
}
