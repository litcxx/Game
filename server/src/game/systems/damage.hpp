#pragma once

#include <cstdint>

#include "config/config.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// The single place where a player loses hp — every damage source goes through it.
// Only an alive target is hit. The damage is clamped to the remaining hp and
// recorded as a HitEvent; at 0 hp the target dies (DEAD, respawn timer started,
// intent cleared) and a DeathEvent is recorded. Returns true if the hit killed.
bool apply_damage(WorldState& state, const GameConfig& config, Player& target, std::uint32_t amount,
                  std::uint32_t attacker_id);
}  // namespace lit::game
