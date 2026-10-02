#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "config/config.hpp"
#include "persist/world_save.hpp"
#include "save.pb.h"
#include "state/world_state.hpp"
#include "systems/capital_system.hpp"
#include "systems/join_system.hpp"
#include "systems/presence_system.hpp"
#include "utils/session_token.hpp"

namespace {

// Season 3 on a 4x4 map: Red's capital in cell 5, radius 1 (cells 1, 4, 5, 6, 9);
// Blue's in cell 15, radius 0.
lit::GameConfig season_config() {
    lit::GameConfig c{};
    c.map_width = 4;
    c.map_height = 4;
    c.season_id = 3;
    c.factions = {{1, "Red", 0xFF0000}, {2, "Blue", 0x0000FF}};
    c.capitals = {{1, 5, 1}, {2, 15, 0}};
    return c;
}

// A world as the World makes it before anyone joins: the map, the capitals'
// zones seeded and protected; its season started at `started_at`.
lit::game::WorldState fresh(const lit::GameConfig& config, std::int64_t started_at = 2'000'000) {
    lit::game::WorldState s;
    s.territory.reset(config.map_width, config.map_height);
    lit::game::seed_capitals(s.territory, config.capitals);
    lit::game::protect_capitals(s.territory, config.capitals);
    s.season_started_at = started_at;
    return s;
}

// A season under way: Ann (Red) in the world with a body, Bob who hasn't chosen,
// Cid (Blue) gone from the world; Red took cell 2, Blue cell 14; a capture of
// cell 3 is half done.
lit::game::WorldState played(const lit::GameConfig& config) {
    auto s = fresh(config, 1'000'000);
    lit::game::create_character(s, "Ann", lit::hash_token("ann-token")).faction_id = 1;
    lit::game::create_character(s, "Bob", lit::hash_token("bob-token"));
    auto& cid = lit::game::create_character(s, "Cid", lit::hash_token("cid-token"));
    cid.faction_id = 2;
    cid.in_world = false;
    s.units[1] = lit::game::Unit{};
    s.territory.set_owner(2, 1);
    s.territory.set_owner(14, 2);
    s.territory.capture_faction[3] = 1;
    s.territory.capture_progress[3] = 50.0;
    return s;
}

// The save as it comes back from a file.
lit::save::WorldSave through_bytes(const lit::save::WorldSave& save) {
    lit::save::WorldSave out;
    EXPECT_TRUE(out.ParseFromString(save.SerializeAsString()));
    return out;
}

}  // namespace

// --- Round trip: what outlives a restart ----------------------------------------

TEST(WorldSaveRoundTrip, TheCharactersComeBackOutOfTheWorld) {
    const auto config = season_config();
    const auto before = played(config);

    auto after = fresh(config);
    ASSERT_EQ(lit::game::restore_world(after, config,
                                       through_bytes(lit::game::make_world_save(before, config))),
              lit::game::SeasonLoad::Continued);

    ASSERT_EQ(after.characters.size(), 3u);
    for (const auto& [id, was] : before.characters) {
        const auto& is = after.characters.at(id);
        EXPECT_EQ(is.id, was.id);
        EXPECT_EQ(is.name, was.name);
        EXPECT_EQ(is.token_hash, was.token_hash);
        EXPECT_EQ(is.faction_id, was.faction_id) << was.name;  // locked for the season
        EXPECT_FALSE(is.in_world) << was.name;                 // until a session brings it back
        EXPECT_FALSE(is.away_until.has_value());
    }
    EXPECT_TRUE(after.units.empty());  // bodies are not saved
    EXPECT_EQ(after.next_character_id, 4u);
    EXPECT_NE(lit::game::find_by_token(after, lit::hash_token("cid-token")), nullptr);
    EXPECT_TRUE(lit::game::is_name_taken(after, "ann"));  // its name stays its own
}

TEST(WorldSaveRoundTrip, TheTerritoryComesBackCountedWithoutTheCaptures) {
    const auto config = season_config();
    const auto before = played(config);

    auto after = fresh(config);
    lit::game::restore_world(after, config,
                             through_bytes(lit::game::make_world_save(before, config)));

    EXPECT_EQ(after.territory.owners, before.territory.owners);
    EXPECT_EQ(after.territory.owned(1), 6u);  // the zone and cell 2
    EXPECT_EQ(after.territory.owned(2), 2u);  // the capital and cell 14
    EXPECT_EQ(after.territory.owned(0), 8u);
    EXPECT_EQ(after.territory.capture_progress[3], 0.0);  // transient
    EXPECT_TRUE(after.territory.protected_cells[5]);      // still a capital's zone
}

TEST(WorldSaveRoundTrip, TheSeasonKeepsItsStart) {
    const auto config = season_config();
    const auto before = played(config);  // started at 1'000'000

    auto after = fresh(config, 2'000'000);
    lit::game::restore_world(after, config,
                             through_bytes(lit::game::make_world_save(before, config)));

    EXPECT_EQ(after.season_started_at, 1'000'000);
}

TEST(WorldSaveFormat, SaysWhatItIs) {
    const auto config = season_config();
    const auto save = lit::game::make_world_save(played(config), config);

    EXPECT_EQ(save.version(), lit::game::kWorldSaveVersion);
    EXPECT_EQ(save.season_id(), 3u);
    EXPECT_EQ(save.map_width(), 4u);
    EXPECT_EQ(save.map_height(), 4u);
    ASSERT_EQ(save.characters_size(), 3);
    EXPECT_EQ(save.characters(0).id(), 1u);  // by id: the same world, the same file
    EXPECT_EQ(save.characters(2).id(), 3u);
}

// --- A new season: the characters stay, the season's own data goes --------------

TEST(WorldSaveNewSeason, TheCharactersStayWithoutAFaction) {
    const auto config = season_config();
    auto save = lit::game::make_world_save(played(config), config);
    save.set_season_id(2);  // last season's

    auto after = fresh(config);
    ASSERT_EQ(lit::game::restore_world(after, config, save), lit::game::SeasonLoad::New);

    ASSERT_EQ(after.characters.size(), 3u);
    for (const auto& [id, c] : after.characters) {
        EXPECT_EQ(c.faction_id, 0u) << c.name;  // chosen again, once
        EXPECT_FALSE(c.in_world);
    }
    EXPECT_EQ(after.characters.at(1).name, "Ann");
    EXPECT_NE(lit::game::find_by_token(after, lit::hash_token("ann-token")), nullptr);
    EXPECT_EQ(after.next_character_id, 4u);  // ids are never reused
}

TEST(WorldSaveNewSeason, TheTerritoryStartsAgainFromTheCapitals) {
    const auto config = season_config();
    auto save = lit::game::make_world_save(played(config), config);
    save.set_season_id(2);

    auto after = fresh(config, 2'000'000);
    lit::game::restore_world(after, config, save);

    EXPECT_EQ(after.territory.owners, fresh(config).territory.owners);
    EXPECT_EQ(after.territory.owned(1), 5u);
    EXPECT_EQ(after.season_started_at, 2'000'000);  // this world's start
}

// A new season may come with a new map.
TEST(WorldSaveNewSeason, TakesAnotherMap) {
    const auto config = season_config();
    auto save = lit::game::make_world_save(played(config), config);
    save.set_season_id(2);
    save.set_map_width(8);
    save.set_owner_faction_ids(std::string(32, '\0'));

    auto after = fresh(config);
    EXPECT_EQ(lit::game::restore_world(after, config, save), lit::game::SeasonLoad::New);
}

// --- A save this server can't take: refused, and the world untouched -------------

namespace {

// Restores `save` into a fresh world: the error, and that nothing changed.
std::string refused(const lit::GameConfig& config, const lit::save::WorldSave& save) {
    auto state = fresh(config);
    const auto untouched = state.territory.owners;
    const auto result = lit::game::restore_world(state, config, save);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(state.characters.empty());
    EXPECT_EQ(state.territory.owners, untouched);
    EXPECT_EQ(state.next_character_id, 1u);
    return result ? std::string{} : result.error();
}

}  // namespace

TEST(WorldSaveRefused, ANewerFormat) {
    const auto config = season_config();
    auto save = lit::game::make_world_save(played(config), config);
    save.set_version(lit::game::kWorldSaveVersion + 1);

    EXPECT_NE(refused(config, save).find("version"), std::string::npos);
}

TEST(WorldSaveRefused, AnotherMapInTheSameSeason) {
    const auto config = season_config();
    auto save = lit::game::make_world_save(played(config), config);
    save.set_map_width(8);
    save.set_owner_faction_ids(std::string(32, '\0'));

    EXPECT_NE(refused(config, save).find("map"), std::string::npos);
}

TEST(WorldSaveRefused, OwnersThatDontFitTheMap) {
    const auto config = season_config();
    auto save = lit::game::make_world_save(played(config), config);
    save.set_owner_faction_ids(std::string(15, '\0'));

    refused(config, save);
}

TEST(WorldSaveRefused, ACellOfAFactionNotInTheConfig) {
    const auto config = season_config();
    auto save = lit::game::make_world_save(played(config), config);
    (*save.mutable_owner_faction_ids())[7] = 9;

    EXPECT_NE(refused(config, save).find("faction 9"), std::string::npos);
}

TEST(WorldSaveRefused, ACharacterOfAFactionNotInTheConfig) {
    const auto config = season_config();
    auto save = lit::game::make_world_save(played(config), config);
    save.mutable_characters(0)->set_faction_id(9);

    EXPECT_NE(refused(config, save).find("faction 9"), std::string::npos);
}

TEST(WorldSaveRefused, ABrokenTokenHash) {
    const auto config = season_config();
    auto save = lit::game::make_world_save(played(config), config);
    save.mutable_characters(1)->mutable_token_hash()->pop_back();

    refused(config, save);
}

TEST(WorldSaveRefused, BadIds) {
    const auto config = season_config();
    const auto good = lit::game::make_world_save(played(config), config);

    auto twice = good;
    twice.mutable_characters(1)->set_id(1);
    refused(config, twice);

    auto zero = good;
    zero.mutable_characters(0)->set_id(0);
    refused(config, zero);

    auto ahead = good;  // an id the next character would get again
    ahead.set_next_player_id(3);
    refused(config, ahead);
}
