#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/world_state.hpp"
#include "sync/snapshot_builder.hpp"
#include "systems/input_system.hpp"
#include "systems/movement_system.hpp"
#include "systems/presence_system.hpp"
#include "systems/spawn_system.hpp"
#include "systems/vision_system.hpp"

namespace {

constexpr double kDt = 1.0 / 60;

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
        lit::game::consume_inputs(state);
        lit::game::integrate_movement(state, config, kDt);
    }
};

}  // namespace

TEST(Presence, CharactersGetNewIdsAndNoBodyUntilTheySpawn) {
    Lobby l;

    const std::uint32_t ann = lit::game::create_character(l.state, "Ann").id;
    const std::uint32_t bob = lit::game::create_character(l.state, "Bob").id;

    EXPECT_EQ(ann, 1u);
    EXPECT_EQ(bob, 2u);
    EXPECT_EQ(l.state.characters.at(ann).name, "Ann");
    EXPECT_TRUE(l.state.units.empty());  // not spawned: no body
}

TEST(Presence, ACharacterLeavesTheWorldWithItsBody) {
    Lobby l;
    const std::uint32_t ann = lit::game::create_character(l.state, "Ann").id;
    lit::game::attach_session(l.state, 7, ann);
    ASSERT_TRUE(lit::game::try_spawn(l.state, l.config, ann, 5, 1));

    EXPECT_EQ(lit::game::detach_session(l.state, 7), ann);
    lit::game::remove_character(l.state, ann);

    EXPECT_TRUE(l.state.characters.empty());
    EXPECT_TRUE(l.state.units.empty());
    EXPECT_TRUE(l.state.sessions.empty());
    EXPECT_EQ(lit::game::detach_session(l.state, 7), std::nullopt);  // already gone
    EXPECT_EQ(lit::game::create_character(l.state, "Bob").id, 2u);   // ids are never reused
}

TEST(Presence, TwoSessionsInARowDriveOneCharacter) {
    Lobby l;
    const std::uint32_t ann = lit::game::create_character(l.state, "Ann").id;

    // The first connection spawns the character, walks it right for three ticks
    // (its seqs 1..3), is told what it sees — and drops.
    lit::game::attach_session(l.state, 1, ann);
    ASSERT_TRUE(lit::game::try_spawn(l.state, l.config, ann, 5, 1));  // cell (1, 1): x = 150
    for (std::uint32_t seq = 1; seq <= 3; ++seq) l.walk_right(1, seq);
    l.state.sessions.at(1).sync.vision = lit::game::compute_vision(l.state, l.config, 1);
    l.state.units.at(ann).hp = 60;  // hurt on the way
    const double x = l.state.units.at(ann).x;
    ASSERT_DOUBLE_EQ(x, 165.0);  // 3 ticks at 5 units

    EXPECT_EQ(lit::game::detach_session(l.state, 1), ann);

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
