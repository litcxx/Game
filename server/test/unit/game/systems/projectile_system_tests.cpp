#include <gtest/gtest.h>

#include <cstdint>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "spatial/spatial_index.hpp"
#include "state/world_state.hpp"
#include "systems/projectile_system.hpp"

namespace {

constexpr auto kAlive = ::game::v1::LIFE_STATE_ALIVE;

// A 10x10-cell map (1000x1000 units); player bodies are 16 units, projectiles
// 8, so a projectile touches a player whose centre is within 24 units.
lit::GameConfig arena_config() {
    lit::GameConfig c{};
    c.map_width = 10;
    c.map_height = 10;
    c.max_hp = 100;
    c.respawn_delay_ticks = 5;
    c.player_radius = 16;
    return c;
}

struct Arena {
    lit::GameConfig config = arena_config();
    lit::game::WorldState state;
    lit::game::SpatialIndex index{1000.0, 1000.0, 200.0};

    // An alive player (session id == player id) with full hp at (x, y).
    lit::game::Player& add_player(std::uint32_t id, std::uint32_t faction, double x, double y) {
        lit::game::Player p;
        p.id = id;
        p.faction_id = faction;
        p.life = kAlive;
        p.hp = 100;
        p.x = x;
        p.y = y;
        return state.players[id] = p;
    }

    // A projectile of `owner` (of `faction`) at (x, y) with velocity (vx, vy)
    // and `range` units of flight left; 30 damage, radius 8.
    void fire(std::uint32_t owner, std::uint32_t faction, double x, double y, double vx, double vy,
              double range) {
        state.projectiles.push_back(lit::game::Projectile{state.next_projectile_id++, owner,
                                                          faction, 30, 8.0, x, y, vx, vy, range});
    }

    // Index this tick's alive players, as World does after movement.
    void index_alive() {
        index.clear();
        for (const auto& [sid, p] : state.players) {
            if (p.life == kAlive) index.insert(sid, p.x, p.y);
        }
    }

    void fly(double dt) { lit::game::update_projectiles(state, config, index, dt); }

    void step(double dt) {
        index_alive();
        fly(dt);
    }

    std::uint32_t hp(std::uint32_t id) { return state.players.at(id).hp; }

    int hits() const {
        int n = 0;
        for (const auto& e : state.events) n += e.has_hit() ? 1 : 0;
        return n;
    }
};

}  // namespace

TEST(Projectiles, FlyStraightAtTheirSpeed) {
    Arena a;
    a.fire(1, 1, 100, 500, 600, 0, 500);

    a.step(0.1);

    ASSERT_EQ(a.state.projectiles.size(), 1u);
    EXPECT_DOUBLE_EQ(a.state.projectiles[0].x, 160.0);
    EXPECT_DOUBLE_EQ(a.state.projectiles[0].y, 500.0);
    EXPECT_DOUBLE_EQ(a.state.projectiles[0].remaining, 440.0);
}

TEST(Projectiles, DespawnAfterTheirRange) {
    Arena a;
    a.fire(1, 1, 100, 500, 600, 0, 50);  // a 60-unit step, only 50 left

    a.step(0.1);

    EXPECT_TRUE(a.state.projectiles.empty());
}

TEST(Projectiles, DespawnWhenLeavingTheMap) {
    Arena a;
    a.fire(1, 1, 990, 500, 600, 0, 500);  // would land at x = 1050

    a.step(0.1);

    EXPECT_TRUE(a.state.projectiles.empty());
}

TEST(Projectiles, HitTheFirstEnemyOnTheirPath) {
    Arena a;
    a.add_player(1, 1, 100, 500);           // shooter
    a.add_player(2, 2, 300, 500);           // first on the path
    a.add_player(3, 2, 380, 500);           // further along
    a.fire(1, 1, 100, 500, 3000, 0, 1000);  // one 300-unit step covers both

    a.step(0.1);

    EXPECT_EQ(a.hp(2), 70u);
    EXPECT_EQ(a.hp(3), 100u);
    EXPECT_TRUE(a.state.projectiles.empty());  // spent on the hit
    ASSERT_EQ(a.state.events.size(), 1u);
    ASSERT_TRUE(a.state.events[0].has_hit());
    EXPECT_EQ(a.state.events[0].hit().attacker_id(), 1u);  // credited to the shooter
    EXPECT_EQ(a.state.events[0].hit().target_id(), 2u);
    EXPECT_EQ(a.state.events[0].hit().damage(), 30u);
}

TEST(Projectiles, PassAlliesAndTheirOwner) {
    Arena a;
    a.add_player(1, 1, 100, 500);  // shooter, overlapping the launch point
    a.add_player(4, 1, 250, 500);  // ally on the path
    a.fire(1, 1, 100, 500, 3000, 0, 1000);

    a.step(0.1);

    EXPECT_EQ(a.hp(1), 100u);
    EXPECT_EQ(a.hp(4), 100u);
    EXPECT_EQ(a.hits(), 0);
    ASSERT_EQ(a.state.projectiles.size(), 1u);
    EXPECT_DOUBLE_EQ(a.state.projectiles[0].x, 400.0);  // flew on
}

TEST(Projectiles, FastProjectileDoesNotTunnel) {
    Arena a;
    a.add_player(2, 2, 300, 500);
    a.fire(1, 1, 100, 500, 6000, 0, 1000);  // one step jumps from x=100 to x=700

    a.step(0.1);

    EXPECT_EQ(a.hp(2), 70u);
}

TEST(Projectiles, IgnoreBodiesKilledEarlierThisTick) {
    Arena a;
    a.add_player(2, 2, 300, 500);
    a.fire(1, 1, 100, 500, 3000, 0, 1000);
    a.index_alive();
    auto& body = a.state.players.at(2);  // killed after indexing (e.g. by melee)
    body.life = ::game::v1::LIFE_STATE_DEAD;
    body.hp = 0;

    a.fly(0.1);

    EXPECT_EQ(a.hits(), 0);
    ASSERT_EQ(a.state.projectiles.size(), 1u);  // not consumed by the body
}

TEST(Projectiles, HitOnlyOneTarget) {
    Arena a;
    a.add_player(2, 2, 300, 500);
    a.add_player(3, 2, 300, 505);  // overlapping the first
    a.fire(1, 1, 100, 500, 3000, 0, 1000);

    a.step(0.1);

    EXPECT_EQ(a.hits(), 1);
    EXPECT_EQ(a.hp(2) + a.hp(3), 170u);  // exactly one of them lost 30
}

TEST(Projectiles, PointBlankOverlapHitsAtOnce) {
    Arena a;
    a.add_player(2, 2, 105, 500);  // overlaps the launch point
    a.fire(1, 1, 100, 500, 800, 0, 500);

    a.step(1.0 / 60.0);

    EXPECT_EQ(a.hp(2), 70u);
}

TEST(Projectiles, ContactWithinTheLastPartialStepCounts) {
    Arena a;
    a.add_player(2, 2, 160, 500);        // touched at x = 136
    a.fire(1, 1, 100, 500, 600, 0, 40);  // last step: x 100 -> 140

    a.step(0.1);

    EXPECT_EQ(a.hp(2), 70u);
}

TEST(Projectiles, NothingIsHitBeyondTheRange) {
    Arena a;
    a.add_player(2, 2, 170, 500);  // would be touched at x = 146, past the range end
    a.fire(1, 1, 100, 500, 600, 0, 40);

    a.step(0.1);

    EXPECT_EQ(a.hp(2), 100u);
    EXPECT_TRUE(a.state.projectiles.empty());
}
