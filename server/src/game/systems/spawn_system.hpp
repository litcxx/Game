#pragma once

#include <cstdint>

#include "config/config.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// Validate and apply a spawn request. Rejected when the player is alive, still
// waiting out its respawn delay, or the cell / faction is invalid. On success the
// player is placed at the cell centre with full hp and cleared intent/cooldowns,
// and true is returned.
bool try_spawn(const WorldState& state, const GameConfig& config, Player& player,
               std::uint32_t cell, std::uint32_t faction_id);
}  // namespace lit::game
