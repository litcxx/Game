#include "persist/world_save.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "utils/session_token.hpp"

namespace lit::game {
namespace {
bool is_faction(const GameConfig& config, std::uint32_t id) {
    return std::ranges::any_of(config.factions,
                               [id](const FactionConfig& faction) { return faction.id == id; });
}

// Everything restore_world() needs from the save, checked before anything changes.
std::expected<void, std::string> check(const GameConfig& config, const ::lit::save::WorldSave& save,
                                       bool same_season) {
    if (save.version() > kWorldSaveVersion) {
        return std::unexpected(
            std::format("the save's format version {} is newer than this "
                        "server's {}",
                        save.version(), kWorldSaveVersion));
    }
    if (same_season) {
        if (save.map_width() != config.map_width || save.map_height() != config.map_height) {
            return std::unexpected(std::format(
                "the map is {}x{} in the save but {}x{} in the config, in the same season {}",
                save.map_width(), save.map_height(), config.map_width, config.map_height,
                config.season_id));
        }
        const std::size_t cells = std::size_t{config.map_width} * config.map_height;
        if (save.owner_faction_ids().size() != cells) {
            return std::unexpected(std::format("{} cell owners for a map of {} cells",
                                               save.owner_faction_ids().size(), cells));
        }
        for (const char owner : save.owner_faction_ids()) {
            const auto faction = static_cast<unsigned char>(owner);
            if (faction != 0 && !is_faction(config, faction)) {
                return std::unexpected(
                    std::format("a cell of faction {}, which the config does not have", faction));
            }
        }
    }
    std::unordered_set<std::uint32_t> ids;
    for (const auto& c : save.characters()) {
        if (c.id() == 0 || c.id() >= save.next_player_id()) {
            return std::unexpected(
                std::format("character id {} is not between 1 and the next id {}", c.id(),
                            save.next_player_id()));
        }
        if (!ids.insert(c.id()).second) {
            return std::unexpected(std::format("character id {} is there twice", c.id()));
        }
        if (c.token_hash().size() != TokenHash{}.size()) {
            return std::unexpected(std::format("character {} has a token hash of {} bytes", c.id(),
                                               c.token_hash().size()));
        }
        if (same_season && c.faction_id() != 0 && !is_faction(config, c.faction_id())) {
            return std::unexpected(
                std::format("character {} is of faction {}, which the config does not have", c.id(),
                            c.faction_id()));
        }
    }
    return {};
}
}  // namespace

::lit::save::WorldSave make_world_save(const WorldState& state, const GameConfig& config) {
    ::lit::save::WorldSave save;
    save.set_version(kWorldSaveVersion);
    save.set_season_id(config.season_id);
    save.set_season_started_at(state.season_started_at);
    save.set_next_player_id(state.next_character_id);
    save.set_map_width(state.territory.width);
    save.set_map_height(state.territory.height);
    const auto& owners = state.territory.owners;
    save.set_owner_faction_ids(std::string(owners.begin(), owners.end()));

    std::vector<const Character*> characters;  // by id: the same world, the same file
    characters.reserve(state.characters.size());
    for (const auto& [id, character] : state.characters) characters.push_back(&character);
    std::ranges::sort(characters, {}, &Character::id);
    for (const Character* character : characters) {
        auto* out = save.add_characters();
        out->set_id(character->id);
        out->set_name(character->name);
        out->set_token_hash(reinterpret_cast<const char*>(character->token_hash.data()),
                            character->token_hash.size());
        out->set_faction_id(character->faction_id);
    }
    return save;
}

std::expected<SeasonLoad, std::string> restore_world(WorldState& state, const GameConfig& config,
                                                     const ::lit::save::WorldSave& save) {
    const bool same_season = save.season_id() == config.season_id;
    if (auto checked = check(config, save, same_season); !checked) {
        return std::unexpected(checked.error());
    }

    if (same_season) {
        const std::string& owners = save.owner_faction_ids();
        for (std::uint32_t index = 0; index < owners.size(); ++index) {
            state.territory.set_owner(index, static_cast<std::uint8_t>(owners[index]));
        }
        state.season_started_at = save.season_started_at();
    }
    for (const auto& c : save.characters()) {
        Character character;
        character.id = c.id();
        character.name = c.name();
        std::ranges::copy(c.token_hash(), character.token_hash.begin());
        character.in_world = false;                               // until a session brings it back
        character.faction_id = same_season ? c.faction_id() : 0;  // a new season: chosen again
        state.characters[c.id()] = std::move(character);
    }
    state.next_character_id = std::max<std::uint32_t>(save.next_player_id(), 1);
    return same_season ? SeasonLoad::Continued : SeasonLoad::New;
}
}  // namespace lit::game
