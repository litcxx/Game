#include "systems/spawn_system.hpp"

#include <algorithm>

#include "state/map_scale.hpp"

namespace lit::game {
std::expected<void, ::game::v1::ErrorCode> try_spawn(WorldState& state, const GameConfig& config,
                                                     std::uint32_t character_id, std::uint32_t cell,
                                                     std::uint32_t faction_id) {
    if (const Unit* body = find_unit(state, character_id)) {
        if (body->life == ::game::v1::LIFE_STATE_ALIVE) {
            return std::unexpected(::game::v1::ERROR_CODE_ALREADY_SPAWNED);
        }
        if (state.tick < body->respawn_tick) {
            return std::unexpected(::game::v1::ERROR_CODE_SPAWN_TOO_EARLY);  // still waiting it out
        }
    }
    if (cell >= config.map_width * config.map_height) {
        return std::unexpected(::game::v1::ERROR_CODE_SPAWN_INVALID_CELL);
    }
    const bool valid_faction = std::ranges::any_of(
        config.factions, [faction_id](const auto& f) { return f.id == faction_id; });
    Character& character = state.characters.at(character_id);
    const bool locked_elsewhere = character.faction_id != 0 && character.faction_id != faction_id;
    if (!valid_faction || locked_elsewhere) {
        return std::unexpected(::game::v1::ERROR_CODE_INVALID_FACTION);
    }
    character.faction_id = faction_id;  // locked for the season (a no-op after the first spawn)

    const std::uint32_t col = cell % config.map_width;
    const std::uint32_t row = cell / config.map_width;
    Unit body;
    body.id = character_id;
    body.faction_id = faction_id;
    body.x = col * kUnitsPerCell + kUnitsPerCell / 2.0;
    body.y = row * kUnitsPerCell + kUnitsPerCell / 2.0;
    body.hp = config.max_hp;
    state.units[character_id] = body;  // a dead body is replaced
    return {};
}
}  // namespace lit::game
