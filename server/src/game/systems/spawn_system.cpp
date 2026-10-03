#include "systems/spawn_system.hpp"

#include <algorithm>
#include <optional>

#include "state/map_scale.hpp"

namespace lit::game {
SpawnPoint capital_spawn_point(const GameConfig& config) {
    return [capitals = config.capitals](const WorldState& /*state*/, std::uint32_t /*character_id*/,
                                        std::uint32_t faction_id) -> std::optional<std::uint32_t> {
        const auto it = std::ranges::find(capitals, faction_id, &CapitalConfig::faction_id);
        if (it == capitals.end()) return std::nullopt;
        return it->cell;
    };
}

std::expected<void, ::game::v1::ErrorCode> try_spawn(WorldState& state, const GameConfig& config,
                                                     std::uint32_t character_id,
                                                     std::uint32_t faction_id,
                                                     const SpawnPoint& spawn_point) {
    std::uint32_t last_attack_input_seq = 0;  // the connection's frames go on counting
    if (const Unit* body = find_unit(state, character_id)) {
        last_attack_input_seq = body->last_attack_input_seq;
        if (body->life == ::game::v1::LIFE_STATE_ALIVE) {
            return std::unexpected(::game::v1::ERROR_CODE_ALREADY_SPAWNED);
        }
        if (state.tick < body->respawn_tick) {
            return std::unexpected(::game::v1::ERROR_CODE_SPAWN_TOO_EARLY);  // still waiting it out
        }
    }
    const bool valid_faction = std::ranges::any_of(
        config.factions, [faction_id](const auto& f) { return f.id == faction_id; });
    Character& character = state.characters.at(character_id);
    const bool locked_elsewhere = character.faction_id != 0 && character.faction_id != faction_id;
    if (!valid_faction || locked_elsewhere) {
        return std::unexpected(::game::v1::ERROR_CODE_INVALID_FACTION);
    }
    const std::optional<std::uint32_t> cell = spawn_point(state, character_id, faction_id);
    if (!cell) {
        return std::unexpected(::game::v1::ERROR_CODE_INVALID_FACTION);  // nowhere to spawn
    }
    character.faction_id = faction_id;  // locked for the season (a no-op after the first spawn)

    const std::uint32_t col = *cell % config.map_width;
    const std::uint32_t row = *cell / config.map_width;
    Unit body;
    body.id = character_id;
    body.faction_id = faction_id;
    body.x = col * kUnitsPerCell + kUnitsPerCell / 2.0;
    body.y = row * kUnitsPerCell + kUnitsPerCell / 2.0;
    body.hp = config.max_hp;
    body.last_attack_input_seq = last_attack_input_seq;
    state.units[character_id] = body;  // a dead body is replaced
    return {};
}
}  // namespace lit::game
