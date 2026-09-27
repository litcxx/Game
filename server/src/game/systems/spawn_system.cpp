#include "systems/spawn_system.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>

#include "state/units.hpp"

namespace lit::game {
bool try_spawn(const WorldState& state, const GameConfig& config, Player& player,
               std::uint32_t cell, std::uint32_t faction_id) {
    if (player.life == ::game::v1::LIFE_STATE_ALIVE) {
        spdlog::warn("spawn ignored (already alive) player_id={}", player.id);
        return false;
    }
    if (player.life == ::game::v1::LIFE_STATE_DEAD && state.tick < player.respawn_tick) {
        spdlog::warn("spawn ignored (respawning) player_id={} respawn_tick={}", player.id,
                     player.respawn_tick);
        return false;  // still waiting out the respawn delay
    }
    if (cell >= config.map_width * config.map_height) {
        spdlog::warn("spawn ignored (invalid cell={}) player_id={}", cell, player.id);
        return false;
    }
    const bool valid_faction = std::ranges::any_of(
        config.factions, [faction_id](const auto& f) { return f.id == faction_id; });
    if (!valid_faction) {
        spdlog::warn("spawn ignored (invalid faction={}) player_id={}", faction_id, player.id);
        return false;
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
    player.respawn_tick = 0;
    player.inputs.clear();  // drop stale pre-spawn commands (last_enqueued_seq stays monotonic)
    return true;
}
}  // namespace lit::game
