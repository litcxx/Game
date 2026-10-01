#include <gtest/gtest.h>

#include <cstdint>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/world_state.hpp"
#include "sync/messages.hpp"
#include "systems/presence_system.hpp"

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

}  // namespace

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
