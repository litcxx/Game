#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/world_state.hpp"
#include "sync/messages.hpp"
#include "systems/presence_system.hpp"
#include "utils/bits.hpp"

namespace {

// Red, Blue and Green on a 4x4 map: Red owns 3 cells, Blue 1, Green none.
struct Season {
    lit::GameConfig config = [] {
        lit::GameConfig c{};
        c.map_width = 4;
        c.map_height = 4;
        c.factions = {{1, "Red", 0xFF0000}, {2, "Blue", 0x0000FF}, {3, "Green", 0x00FF00}};
        return c;
    }();
    lit::game::WorldState state = [] {
        lit::game::WorldState s;
        s.territory.reset(4, 4);
        s.territory.set_owner(0, 1);
        s.territory.set_owner(1, 1);
        s.territory.set_owner(4, 1);
        s.territory.set_owner(15, 2);
        return s;
    }();

    // A character in the world, of `faction` (0: none chosen yet).
    lit::game::Character& player(const char* name, std::uint32_t faction) {
        auto& c = lit::game::create_character(state, name, {});
        c.faction_id = faction;
        return c;
    }
};

// The sight of a client that sees exactly `cells` of the 4x4 map.
lit::game::Vision sees(std::initializer_list<std::uint32_t> cells) {
    lit::game::Vision v;
    v.cells.assign(16, 0);
    for (std::uint32_t index : cells) v.cells[index] = 1;
    return v;
}

// MapState.explored, a flag per cell.
std::vector<std::uint8_t> explored(const ::game::v1::ServerMessage& msg) {
    return lit::unpack_bits(msg.map_state().explored(), 16);
}

std::uint8_t owner(const ::game::v1::ServerMessage& msg, std::uint32_t index) {
    return static_cast<std::uint8_t>(msg.map_state().owner_faction_ids().at(index));
}

}  // namespace

// --- MapState: the map as the recipient's faction knows it (GAME-020) -------------

// Red saw the top row and cell 4 when cell 3 was neutral; Blue has taken it since,
// unseen. A Red player coming back sees nothing yet: the map is Red's memory.
TEST(MapStateMessage, TheFactionsExploredCellsWithTheOwnersItLastSaw) {
    Season s;
    s.state.memory[1].see(s.state.territory, sees({0, 1, 2, 3, 4}));
    s.state.territory.set_owner(3, 2);

    const auto msg = lit::game::make_map_state(s.state, 1, lit::game::Vision{});

    ASSERT_TRUE(msg.has_map_state());
    const std::vector<std::uint8_t> expected{1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(explored(msg), expected);
    EXPECT_EQ(owner(msg, 0), 1);
    EXPECT_EQ(owner(msg, 1), 1);
    EXPECT_EQ(owner(msg, 3), 0);   // as last seen, not Blue's now
    EXPECT_EQ(owner(msg, 15), 0);  // Blue's, never seen: unknown
}

// What the client sees now is as it is now, with the captures under way there;
// a capture in a cell explored but out of sight is not told.
TEST(MapStateMessage, VisibleCellsAsTheyAreNowWithTheirCaptures) {
    Season s;
    s.state.memory[1].see(s.state.territory, sees({5, 6}));
    s.state.territory.set_owner(5, 2);
    for (std::uint32_t index : {5u, 6u}) {
        s.state.territory.capture_faction[index] = 1;
        s.state.territory.capture_progress[index] = 40.0;
        s.state.territory.active.insert(index);
    }

    const auto msg = lit::game::make_map_state(s.state, 1, sees({5}));

    EXPECT_EQ(owner(msg, 5), 2);  // now
    ASSERT_EQ(msg.map_state().captures_size(), 1);
    EXPECT_EQ(msg.map_state().captures(0).index(), 5u);
    EXPECT_EQ(explored(msg)[5], 1);
    EXPECT_EQ(explored(msg)[6], 1);
}

TEST(MapStateMessage, NoFactionKnowsNothing) {
    Season s;
    s.state.memory[1].see(s.state.territory, sees({0, 1, 4}));

    const auto msg = lit::game::make_map_state(s.state, 0, lit::game::Vision{});

    EXPECT_TRUE(msg.map_state().explored().empty());
    for (const char o : msg.map_state().owner_faction_ids()) EXPECT_EQ(o, 0);
}

// A faction's memory is its own: Blue has seen nothing, whatever Red knows.
TEST(MapStateMessage, AFactionKnowsOnlyWhatItHasSeen) {
    Season s;
    s.state.memory[1].see(s.state.territory, sees({0, 1, 4}));

    const auto msg = lit::game::make_map_state(s.state, 2, lit::game::Vision{});

    EXPECT_TRUE(msg.map_state().explored().empty());
    for (const char o : msg.map_state().owner_faction_ids()) EXPECT_EQ(o, 0);
}

TEST(FactionScoresMessage, EveryFactionInTheWelcomesOrderWithItsCells) {
    const Season s;

    const auto msg = lit::game::make_faction_scores(s.state, s.config);

    ASSERT_TRUE(msg.has_faction_scores());
    const auto& scores = msg.faction_scores().scores();
    ASSERT_EQ(scores.size(), 3);
    EXPECT_EQ(scores[0].faction_id(), 1u);
    EXPECT_EQ(scores[0].cells(), 3u);
    EXPECT_EQ(scores[1].faction_id(), 2u);
    EXPECT_EQ(scores[1].cells(), 1u);
    EXPECT_EQ(scores[2].faction_id(), 3u);
    EXPECT_EQ(scores[2].cells(), 0u);
}

// Online: the faction's characters in the world, as the roster lists them — the
// fallen and the ones away in their grace too; not who has left, nor who has
// not chosen yet.
TEST(FactionScoresMessage, OnlineIsTheFactionsCharactersInTheWorld) {
    Season s;
    s.player("ann", 1);
    s.player("bob", 1).away_until = 900;  // away, in its grace
    s.player("cid", 2);
    s.player("dan", 2).in_world = false;  // left
    s.player("eve", 0);                   // hasn't chosen

    const auto msg = lit::game::make_faction_scores(s.state, s.config);

    const auto& scores = msg.faction_scores().scores();
    ASSERT_EQ(scores.size(), 3);
    EXPECT_EQ(scores[0].online(), 2u);
    EXPECT_EQ(scores[1].online(), 1u);
    EXPECT_EQ(scores[2].online(), 0u);
}
