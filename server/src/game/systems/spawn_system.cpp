#include "systems/spawn_system.hpp"

#include <algorithm>

#include "state/units.hpp"

namespace lit::game {
std::expected<void, ::game::v1::ErrorCode> try_spawn(const WorldState& state,
                                                     const GameConfig& config, Player& player,
                                                     std::uint32_t cell, std::uint32_t faction_id) {
    if (player.life == ::game::v1::LIFE_STATE_ALIVE) {
        return std::unexpected(::game::v1::ERROR_CODE_ALREADY_SPAWNED);
    }
    if (player.life == ::game::v1::LIFE_STATE_DEAD && state.tick < player.respawn_tick) {
        return std::unexpected(::game::v1::ERROR_CODE_SPAWN_TOO_EARLY);  // still waiting it out
    }
    if (cell >= config.map_width * config.map_height) {
        return std::unexpected(::game::v1::ERROR_CODE_SPAWN_INVALID_CELL);
    }
    const bool valid_faction = std::ranges::any_of(
        config.factions, [faction_id](const auto& f) { return f.id == faction_id; });
    if (!valid_faction) {
        return std::unexpected(::game::v1::ERROR_CODE_INVALID_FACTION);
    }

    const std::uint32_t col = cell % config.map_width;
    const std::uint32_t row = cell / config.map_width;
    player.faction_id = faction_id;
    player.x = col * kUnitsPerCell + kUnitsPerCell / 2.0;
    player.y = row * kUnitsPerCell + kUnitsPerCell / 2.0;
    player.hp = config.max_hp;
    player.life = ::game::v1::LIFE_STATE_ALIVE;
    player.move_x = 0;
    player.move_y = 0;
    player.capturing = false;
    player.attack = false;
    player.attack_ready_tick = 0;
    player.cooldown_ticks = 0;
    player.block_until_tick = 0;
    player.block_ready_tick = 0;
    player.block_cooldown_ticks = 0;
    player.respawn_tick = 0;
    player.inputs.clear();  // drop stale pre-spawn commands (last_enqueued_seq stays monotonic)
    return {};
}
}  // namespace lit::game
