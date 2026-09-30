#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "config/config.hpp"
#include "game/spawn_at.hpp"
#include "game/v1/protocol.pb.h"
#include "state/world_state.hpp"
#include "sync/snapshot_builder.hpp"
#include "systems/input_system.hpp"
#include "systems/movement_system.hpp"
#include "systems/presence_system.hpp"
#include "systems/spawn_system.hpp"
#include "systems/vision_system.hpp"
#include "utils/session_token.hpp"

namespace {

constexpr double kDt = 1.0 / 60;

// A token hash told apart by its first byte.
lit::TokenHash token(std::uint8_t n) {
    lit::TokenHash hash{};
    hash[0] = n;
    return hash;
}

// A 4x4-cell map whose every cell is in sight of any body.
struct Lobby {
    lit::GameConfig config = [] {
        lit::GameConfig c{};
        c.map_width = 4;
        c.map_height = 4;
        c.move_speed = 300;
        c.max_hp = 100;
        c.vision_radius = 1000;
        c.factions = {{1, "Red", 0xFF0000}, {2, "Blue", 0x0000FF}};
        return c;
    }();
    lit::game::WorldState state = [] {
        lit::game::WorldState s;
        s.territory.reset(4, 4);
        return s;
    }();

    // The session's client sends one frame moving right; the tick applies it.
    void walk_right(std::uint64_t session_id, std::uint32_t seq) {
        ::game::v1::Input input;
        auto* frame = input.add_frames();
        frame->set_seq(seq);
        frame->set_move_x(1);
        lit::game::enqueue_frames(state.sessions.at(session_id).input, input, config.limits);
        tick();
    }

    // A tick of input and movement with no new frames.
    void tick() {
        lit::game::consume_inputs(state);
        lit::game::integrate_movement(state, config, kDt);
    }

    // A character with a session and a body at cell 5 (x = 150).
    std::uint32_t join_and_spawn(const char* name, std::uint8_t token_n, std::uint64_t session_id) {
        const std::uint32_t id = lit::game::create_character(state, name, token(token_n)).id;
        lit::game::attach_session(state, session_id, id);
        EXPECT_TRUE(lit::game::try_spawn(state, config, id, 1, lit::test::spawn_at(5)));
        return id;
    }
};

}  // namespace

TEST(Presence, CharactersGetNewIdsAndNoBodyUntilTheySpawn) {
    Lobby l;

    const std::uint32_t ann = lit::game::create_character(l.state, "Ann", token(1)).id;
    const std::uint32_t bob = lit::game::create_character(l.state, "Bob", token(2)).id;

    EXPECT_EQ(ann, 1u);
    EXPECT_EQ(bob, 2u);
    const lit::game::Character& a = l.state.characters.at(ann);
    EXPECT_EQ(a.name, "Ann");
    EXPECT_EQ(a.token_hash, token(1));
    EXPECT_TRUE(a.in_world);
    EXPECT_EQ(a.away_until, std::nullopt);
    EXPECT_TRUE(l.state.units.empty());  // not spawned: no body
}

TEST(Presence, ADroppedCharacterStaysInTheWorldStandingStill) {
    Lobby l;
    const std::uint32_t ann = l.join_and_spawn("Ann", 1, 7);
    l.walk_right(7, 1);
    const double x = l.state.units.at(ann).x;

    EXPECT_EQ(lit::game::detach_session(l.state, 7, /*away_until=*/100), ann);

    const lit::game::Character& a = l.state.characters.at(ann);
    EXPECT_TRUE(a.in_world);
    EXPECT_EQ(a.away_until, 100u);
    EXPECT_TRUE(l.state.sessions.empty());
    ASSERT_TRUE(l.state.units.contains(ann));  // visible and vulnerable
    l.tick();
    EXPECT_DOUBLE_EQ(l.state.units.at(ann).x, x);  // its last move does not repeat
    EXPECT_EQ(lit::game::detach_session(l.state, 7, 100), std::nullopt);  // already detached
}

TEST(Presence, TheCharacterLeavesWhenItsGraceEndsAndItsRecordStays) {
    Lobby l;
    const std::uint32_t ann = l.join_and_spawn("Ann", 1, 7);
    lit::game::detach_session(l.state, 7, /*away_until=*/100);

    EXPECT_TRUE(lit::game::leave_after_grace(l.state, 99).empty());
    EXPECT_EQ(lit::game::leave_after_grace(l.state, 100), std::vector<std::uint32_t>{ann});

    const lit::game::Character& a = l.state.characters.at(ann);  // the record stays
    EXPECT_FALSE(a.in_world);
    EXPECT_EQ(a.away_until, std::nullopt);
    EXPECT_EQ(a.name, "Ann");
    EXPECT_EQ(a.token_hash, token(1));
    EXPECT_TRUE(l.state.units.empty());                                       // the body is gone
    EXPECT_TRUE(lit::game::leave_after_grace(l.state, 200).empty());          // left once
    EXPECT_EQ(lit::game::create_character(l.state, "Bob", token(2)).id, 2u);  // ids never reused
}

TEST(Presence, GraceLeaversComeInIdOrder) {
    Lobby l;
    for (std::uint64_t session = 1; session <= 3; ++session) {
        const auto id = lit::game::create_character(l.state, "x", token(0)).id;
        lit::game::attach_session(l.state, session, id);
        lit::game::detach_session(l.state, session, 10);
    }

    EXPECT_EQ(lit::game::leave_after_grace(l.state, 10), (std::vector<std::uint32_t>{1, 2, 3}));
}

TEST(Presence, AReturningSessionEndsTheAbsence) {
    Lobby l;
    const std::uint32_t ann = l.join_and_spawn("Ann", 1, 7);
    lit::game::detach_session(l.state, 7, 100);

    EXPECT_EQ(lit::game::attach_session(l.state, 8, ann), std::nullopt);  // replaced no one

    EXPECT_EQ(l.state.characters.at(ann).away_until, std::nullopt);
    EXPECT_TRUE(lit::game::leave_after_grace(l.state, 200).empty());
    EXPECT_EQ(l.state.sessions.at(8).character_id, ann);
}

TEST(Presence, ASecondSessionReplacesTheFirst) {
    Lobby l;
    const std::uint32_t ann = l.join_and_spawn("Ann", 1, 7);

    EXPECT_EQ(lit::game::attach_session(l.state, 8, ann), 7u);

    EXPECT_FALSE(l.state.sessions.contains(7));  // it drives nothing now
    EXPECT_EQ(l.state.sessions.at(8).character_id, ann);
    EXPECT_TRUE(l.state.characters.at(ann).in_world);
    EXPECT_EQ(l.state.characters.at(ann).away_until, std::nullopt);
    EXPECT_EQ(lit::game::detach_session(l.state, 7, 100),
              std::nullopt);  // its close changes nothing
}

TEST(Presence, ACharacterComesBackAfterLeaving) {
    Lobby l;
    const std::uint32_t ann = l.join_and_spawn("Ann", 1, 7);
    lit::game::detach_session(l.state, 7, 100);
    lit::game::leave_after_grace(l.state, 100);

    lit::game::attach_session(l.state, 9, ann);

    EXPECT_TRUE(l.state.characters.at(ann).in_world);
    EXPECT_FALSE(l.state.units.contains(ann));  // not spawned
    EXPECT_EQ(l.state.sessions.at(9).character_id, ann);
}

TEST(Presence, LeavingTheWorldNowTakesTheBody) {
    Lobby l;
    const std::uint32_t ann = l.join_and_spawn("Ann", 1, 7);
    lit::game::detach_session(l.state, 7, 100);

    lit::game::leave_world(l.state, ann);

    EXPECT_FALSE(l.state.characters.at(ann).in_world);
    EXPECT_EQ(l.state.characters.at(ann).away_until, std::nullopt);
    EXPECT_TRUE(l.state.units.empty());
}

TEST(Presence, FindsACharacterByItsToken) {
    Lobby l;
    const std::uint32_t ann = lit::game::create_character(l.state, "Ann", token(1)).id;
    const std::uint32_t bob = lit::game::create_character(l.state, "Bob", token(2)).id;
    lit::game::leave_world(l.state, bob);

    ASSERT_NE(lit::game::find_by_token(l.state, token(1)), nullptr);
    EXPECT_EQ(lit::game::find_by_token(l.state, token(1))->id, ann);
    ASSERT_NE(lit::game::find_by_token(l.state, token(2)), nullptr);  // out of the world too
    EXPECT_EQ(lit::game::find_by_token(l.state, token(2))->id, bob);
    EXPECT_EQ(lit::game::find_by_token(l.state, token(3)), nullptr);
}

TEST(Presence, TwoSessionsInARowDriveOneCharacter) {
    Lobby l;
    const std::uint32_t ann = lit::game::create_character(l.state, "Ann", token(1)).id;

    // The first connection spawns the character, walks it right for three ticks
    // (its seqs 1..3), is told what it sees — and drops.
    lit::game::attach_session(l.state, 1, ann);
    ASSERT_TRUE(
        lit::game::try_spawn(l.state, l.config, ann, 1, lit::test::spawn_at(5)));  // x = 150
    for (std::uint32_t seq = 1; seq <= 3; ++seq) l.walk_right(1, seq);
    l.state.sessions.at(1).sync.vision = lit::game::compute_vision(l.state, l.config, 1);
    l.state.units.at(ann).hp = 60;  // hurt on the way
    const double x = l.state.units.at(ann).x;
    ASSERT_DOUBLE_EQ(x, 165.0);  // 3 ticks at 5 units

    EXPECT_EQ(lit::game::detach_session(l.state, 1, 100), ann);

    // The character stays in the world, and its body with it.
    ASSERT_TRUE(l.state.characters.contains(ann));
    ASSERT_TRUE(l.state.units.contains(ann));
    EXPECT_EQ(l.state.units.at(ann).hp, 60u);
    EXPECT_EQ(l.state.units.at(ann).faction_id, 1u);

    // A second connection drives the same character, starting afresh: nothing
    // queued, nothing told, and its own seqs from 1 are taken.
    lit::game::attach_session(l.state, 2, ann);
    const lit::game::ClientSession& second = l.state.sessions.at(2);
    EXPECT_EQ(second.character_id, ann);
    EXPECT_TRUE(second.input.commands.empty());
    EXPECT_EQ(second.input.last_input_seq, 0u);
    EXPECT_TRUE(second.sync.vision.cells.empty());

    l.walk_right(2, 1);
    EXPECT_EQ(second.input.last_input_seq, 1u);
    EXPECT_DOUBLE_EQ(l.state.units.at(ann).x, x + 5.0);  // the same body walks on

    // Its first snapshot is about that body and reveals all it sees anew.
    const auto vision = lit::game::compute_vision(l.state, l.config, 1);
    const auto msg = lit::game::build_snapshot(l.state, second, vision, false);
    const auto& snap = msg.snapshot();
    EXPECT_EQ(snap.you().life(), ::game::v1::LIFE_STATE_ALIVE);
    EXPECT_EQ(snap.you().last_input_seq(), 1u);
    ASSERT_EQ(snap.players_size(), 1);
    EXPECT_EQ(snap.players(0).id(), ann);
    EXPECT_EQ(snap.players(0).hp(), 60u);
    EXPECT_EQ(snap.revealed_size(), 16);  // the whole map, as for a new client
}
