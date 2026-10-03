#include <gtest/gtest.h>

#include <cstdint>

#include "config/config.hpp"
#include "game/spawn_at.hpp"
#include "game/v1/protocol.pb.h"
#include "spatial/spatial_index.hpp"
#include "state/world_state.hpp"
#include "systems/combat_system.hpp"
#include "systems/input_system.hpp"
#include "systems/movement_system.hpp"
#include "systems/presence_system.hpp"
#include "systems/projectile_system.hpp"
#include "systems/spawn_system.hpp"

namespace {

constexpr auto kAlive = ::game::v1::LIFE_STATE_ALIVE;
constexpr double kDt = 1.0 / 60;

// A 10x10-cell map. "Strike": melee, range 120, 40 damage, cooldown 10.
// "Shot": projectile, 30 damage, range 500, 600 units/s, radius 8.
lit::GameConfig combat_config() {
    lit::GameConfig c{};
    c.map_width = 10;
    c.map_height = 10;
    c.move_speed = 300;
    c.max_hp = 100;
    c.respawn_delay_ticks = 5;
    c.player_radius = 16;
    c.factions = {{1, "Red", 0xFF0000}, {2, "Blue", 0x0000FF}};
    c.abilities = {
        {1, lit::AbilityKind::Melee, "Strike", 10, 40, 120, 0, 0, 0},
        {2, lit::AbilityKind::Projectile, "Shot", 30, 30, 500, 600, 8, 0},
    };
    return c;
}

// A player — a character, the session driving it and its body, spawned at
// (450, 450) — and, 100 units to its right, a monster stand-in: a unit of
// another faction that no character or session owns, whose intent the test
// writes (as an AI will).
struct Encounter {
    static constexpr std::uint32_t kPlayer = 1;       // the first character id
    static constexpr std::uint32_t kStub = 1U << 24;  // world entities' ids start here
    static constexpr std::uint64_t kSession = 7;

    lit::GameConfig config = combat_config();
    lit::game::WorldState state;
    lit::game::SpatialIndex index{1000.0, 1000.0, 200.0};

    Encounter() {
        state.territory.reset(10, 10);
        EXPECT_EQ(lit::game::create_character(state, "Ann", {}).id, kPlayer);
        lit::game::attach_session(state, kSession, kPlayer);
        EXPECT_TRUE(
            lit::game::try_spawn(state, config, kPlayer, 1, lit::test::spawn_at(44)));  // (4, 4)
        state.units[kStub] = stub_body();
    }

    static lit::game::Unit stub_body() {
        lit::game::Unit u;
        u.id = kStub;
        u.faction_id = 2;
        u.x = 550.0;
        u.y = 450.0;
        u.hp = 100;
        return u;
    }

    lit::game::Unit& player() { return state.units.at(kPlayer); }
    lit::game::Unit& stub() { return state.units.at(kStub); }

    // The player's client sends one input frame.
    void player_input(std::uint32_t input_seq, const lit::game::Intent& intent) {
        ::game::v1::Input input;
        auto* f = input.add_frames();
        f->set_input_seq(input_seq);
        f->set_move_x(intent.move_x);
        f->set_move_y(intent.move_y);
        f->set_attack(intent.attack);
        f->set_ability_id(intent.ability_id);
        f->set_aim_x(intent.aim_x);
        f->set_aim_y(intent.aim_y);
        lit::game::enqueue_frames(state.sessions.at(kSession).input, input, config.limits);
    }

    // One tick of the unit systems, in World's order.
    void tick() {
        ++state.tick;
        lit::game::consume_inputs(state);
        lit::game::integrate_movement(state, config, kDt);
        index.clear();
        for (const auto& [id, u] : state.units) {
            if (u.life == kAlive) index.insert(id, u.x, u.y);
        }
        lit::game::activate_blocks(state, config);
        lit::game::resolve_attacks(state, config, index);
        lit::game::update_projectiles(state, config, index, kDt);
    }

    const ::game::v1::GameEvent* first_death() const {
        for (const auto& e : state.events) {
            if (e.has_death()) return &e;
        }
        return nullptr;
    }
};

}  // namespace

TEST(UnitWithoutCharacter, StrikesAPlayerWithTheSameAbility) {
    Encounter e;
    e.stub().intent.attack = true;  // its "AI" swings the melee Strike
    e.stub().intent.ability_id = 1;

    e.tick();

    EXPECT_EQ(e.player().hp, 60u);  // Strike: 40
    ASSERT_EQ(e.state.events.size(), 2u);
    ASSERT_TRUE(e.state.events[0].has_hit());
    EXPECT_EQ(e.state.events[0].hit().attacker_id(), Encounter::kStub);
    EXPECT_EQ(e.state.events[0].hit().target_id(), Encounter::kPlayer);
    ASSERT_TRUE(e.state.events[1].has_ability());
    EXPECT_EQ(e.state.events[1].ability().player_id(), Encounter::kStub);
    EXPECT_EQ(e.stub().attack_ready_tick, e.state.tick + 10);  // the shared cooldown
}

// A strike reaches bodies, not centres: the target's edge within the range is hit,
// as a projectile hits on touching the body.
TEST(MeleeStrike, HitsATargetWhoseBodyTouchesTheRange) {
    Encounter e;
    e.stub().x = e.player().x + 120 + 16;  // Strike's range + player_radius
    e.stub().intent.attack = true;
    e.stub().intent.ability_id = 1;

    e.tick();

    EXPECT_EQ(e.player().hp, 60u);  // Strike: 40
}

TEST(MeleeStrike, MissesATargetJustBeyondTheRangeAndBody) {
    Encounter e;
    e.stub().x = e.player().x + 120 + 16 + 1;
    e.stub().intent.attack = true;
    e.stub().intent.ability_id = 1;

    e.tick();

    EXPECT_EQ(e.player().hp, 100u);
}

TEST(UnitWithoutCharacter, IsKilledByAPlayerLikeAnyBody) {
    Encounter e;
    e.stub().hp = 40;  // one Strike

    e.player_input(1, {.attack = true, .ability_id = 1});
    e.tick();

    EXPECT_EQ(e.stub().hp, 0u);
    EXPECT_EQ(e.stub().life, ::game::v1::LIFE_STATE_DEAD);
    const auto* death = e.first_death();
    ASSERT_NE(death, nullptr);
    EXPECT_EQ(death->death().victim_id(), Encounter::kStub);
    EXPECT_EQ(death->death().killer_id(), Encounter::kPlayer);
}

TEST(UnitWithoutCharacter, IsHitByAPlayersProjectile) {
    Encounter e;
    e.player_input(1, {.attack = true, .ability_id = 2, .aim_x = 1});

    for (int i = 0; i < 12; ++i) e.tick();  // 100 units at 10 per tick

    EXPECT_EQ(e.stub().hp, 70u);  // Shot: 30
    EXPECT_TRUE(e.state.projectiles.empty());
}

TEST(UnitWithoutCharacter, BlocksAndMovesByItsOwnIntent) {
    Encounter e;
    e.config.abilities.push_back({3, lit::AbilityKind::Block, "Guard", 10, 0, 0, 0, 0, 9});
    e.stub().intent = {.move_x = 1, .attack = true, .ability_id = 3};  // walks right, guarding
    e.player_input(1, {.move_x = -1, .attack = true, .ability_id = 1});

    e.tick();

    EXPECT_EQ(e.stub().intent.move_x, 1);   // player input drives the player only
    EXPECT_DOUBLE_EQ(e.stub().x, 555.0);    // 300 units/s for 1/60 s
    EXPECT_DOUBLE_EQ(e.player().x, 445.0);  // and the player walked left
    EXPECT_EQ(e.stub().hp, 100u);           // the Strike was blocked
    EXPECT_EQ(e.stub().block_until_tick, e.state.tick + 9);
}

// --- Which input frame an attack came from (LTC-89) ------------------------------

TEST(AttackInputSeq, AShotCarriesTheFrameThatLaunchedIt) {
    Encounter e;
    e.player_input(7, {.attack = true, .ability_id = 2, .aim_y = -1});  // up, away from the stub

    e.tick();

    ASSERT_EQ(e.state.projectiles.size(), 1u);
    EXPECT_EQ(e.state.projectiles[0].input_seq, 7u);
    EXPECT_EQ(e.player().last_attack_input_seq, 7u);
}

TEST(AttackInputSeq, AStrikeIsAnAttackTooABlockIsNot) {
    Encounter e;
    e.config.abilities.push_back({3, lit::AbilityKind::Block, "Guard", 10, 0, 0, 0, 0, 9});

    e.player_input(3, {.attack = true, .ability_id = 1});
    e.tick();
    EXPECT_EQ(e.player().last_attack_input_seq, 3u);

    e.player_input(4, {.attack = true, .ability_id = 3});
    e.tick();
    EXPECT_EQ(e.player().block_until_tick, e.state.tick + 9);  // guarding
    EXPECT_EQ(e.player().last_attack_input_seq, 3u);           // still the strike
}

TEST(AttackInputSeq, NoAimNoShotAndNoAttack) {
    Encounter e;

    e.player_input(2, {.attack = true, .ability_id = 2});  // aim (0, 0)
    e.tick();

    EXPECT_TRUE(e.state.projectiles.empty());
    EXPECT_EQ(e.player().last_attack_input_seq, 0u);
    EXPECT_EQ(e.player().attack_ready_tick, 0u);
}

// Held attack, then the frames stop: frame 2 found the attack on cooldown, and
// its repeats do not shoot once the cooldown is over — no shot without its frame.
TEST(AttackInputSeq, ARepeatedIntentDoesNotAttack) {
    Encounter e;
    const lit::game::Intent shoot{.attack = true, .ability_id = 2, .aim_y = -1};
    e.player_input(1, shoot);
    e.tick();
    e.player_input(2, shoot);
    e.tick();

    for (int i = 0; i < 40; ++i) e.tick();  // past the 30-tick cooldown, no frames

    ASSERT_EQ(e.state.projectiles.size(), 1u);  // the first, still flying (50 ticks)
    EXPECT_EQ(e.player().last_attack_input_seq, 1u);
}

// A new connection numbers its frames from 1 again: its shots in flight and its
// last attack no longer refer to its frames.
TEST(AttackInputSeq, ANewConnectionForgetsTheOldFrames) {
    Encounter e;
    e.player_input(9, {.attack = true, .ability_id = 2, .aim_y = -1});
    e.tick();
    ASSERT_EQ(e.state.projectiles.size(), 1u);

    lit::game::attach_session(e.state, Encounter::kSession + 1, Encounter::kPlayer);

    EXPECT_EQ(e.state.projectiles[0].input_seq, 0u);
    EXPECT_EQ(e.player().last_attack_input_seq, 0u);
}

// A respawn on the same connection keeps counting its frames: so does the body.
TEST(AttackInputSeq, ARespawnKeepsTheLastAttack) {
    Encounter e;
    e.player_input(5, {.attack = true, .ability_id = 1});
    e.tick();
    e.player().life = ::game::v1::LIFE_STATE_DEAD;
    e.player().respawn_tick = e.state.tick;

    ASSERT_TRUE(
        lit::game::try_spawn(e.state, e.config, Encounter::kPlayer, 1, lit::test::spawn_at(44)));

    EXPECT_EQ(e.player().life, kAlive);
    EXPECT_EQ(e.player().last_attack_input_seq, 5u);
}
