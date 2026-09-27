#include <gtest/gtest.h>

#include <cstdint>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/world_state.hpp"
#include "systems/damage.hpp"

namespace {

lit::GameConfig damage_config() {
    lit::GameConfig c{};
    c.max_hp = 100;
    c.respawn_delay_ticks = 5;
    return c;
}

// An alive player that is moving, capturing and attacking.
lit::game::Player alive_player(std::uint32_t id, std::uint32_t hp) {
    lit::game::Player p;
    p.id = id;
    p.faction_id = 1;
    p.life = ::game::v1::LIFE_STATE_ALIVE;
    p.hp = hp;
    p.move_x = 1;
    p.move_y = -1;
    p.capturing = true;
    p.attack = true;
    return p;
}

}  // namespace

TEST(Damage, NonLethalHitReducesHpAndRecordsHitEvent) {
    lit::game::WorldState state;
    state.tick = 7;
    auto config = damage_config();
    auto target = alive_player(2, 100);

    const bool killed = lit::game::apply_damage(state, config, target, 30, 1);

    EXPECT_FALSE(killed);
    EXPECT_EQ(target.hp, 70u);
    EXPECT_EQ(target.life, ::game::v1::LIFE_STATE_ALIVE);
    ASSERT_EQ(state.events.size(), 1u);
    const auto& ev = state.events[0];
    EXPECT_EQ(ev.tick(), 7u);
    ASSERT_TRUE(ev.has_hit());
    EXPECT_EQ(ev.hit().attacker_id(), 1u);
    EXPECT_EQ(ev.hit().target_id(), 2u);
    EXPECT_EQ(ev.hit().damage(), 30u);
}

TEST(Damage, OverkillIsClampedToRemainingHp) {
    lit::game::WorldState state;
    auto config = damage_config();
    auto target = alive_player(2, 15);

    lit::game::apply_damage(state, config, target, 40, 1);

    EXPECT_EQ(target.hp, 0u);  // no unsigned wrap-around
    ASSERT_FALSE(state.events.empty());
    ASSERT_TRUE(state.events[0].has_hit());
    EXPECT_EQ(state.events[0].hit().damage(), 15u);  // what was actually removed
}

TEST(Damage, LethalHitKillsTargetAndRecordsDeath) {
    lit::game::WorldState state;
    state.tick = 100;
    auto config = damage_config();
    auto target = alive_player(2, 30);

    const bool killed = lit::game::apply_damage(state, config, target, 30, 1);

    EXPECT_TRUE(killed);
    EXPECT_EQ(target.hp, 0u);
    EXPECT_EQ(target.life, ::game::v1::LIFE_STATE_DEAD);
    EXPECT_EQ(target.respawn_tick, 105u);  // tick + respawn_delay_ticks
    // A body keeps no intent.
    EXPECT_EQ(target.move_x, 0);
    EXPECT_EQ(target.move_y, 0);
    EXPECT_FALSE(target.capturing);
    EXPECT_FALSE(target.attack);

    ASSERT_EQ(state.events.size(), 2u);  // hit, then death
    EXPECT_TRUE(state.events[0].has_hit());
    const auto& death_ev = state.events[1];
    EXPECT_EQ(death_ev.tick(), 100u);
    ASSERT_TRUE(death_ev.has_death());
    EXPECT_EQ(death_ev.death().victim_id(), 2u);
    EXPECT_EQ(death_ev.death().killer_id(), 1u);
}

TEST(Damage, TargetThatIsNotAliveIsIgnored) {
    lit::game::WorldState state;
    auto config = damage_config();
    auto body = alive_player(2, 0);
    body.life = ::game::v1::LIFE_STATE_DEAD;
    body.respawn_tick = 50;

    const bool killed = lit::game::apply_damage(state, config, body, 30, 1);

    EXPECT_FALSE(killed);
    EXPECT_EQ(body.respawn_tick, 50u);  // not killed a second time
    EXPECT_TRUE(state.events.empty());  // no hit on a body
}
