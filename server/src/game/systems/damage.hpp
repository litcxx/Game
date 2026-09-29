#pragma once

#include <cstdint>

#include "config/config.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// The single place where a unit loses hp — every damage source goes through it.
// Only an alive target is hit. A blocking target (tick < block_until_tick) takes
// nothing: the hit is recorded as a blocked HitEvent (damage 0) and returns false.
// The damage is clamped to the remaining hp and recorded as a HitEvent; at 0 hp
// the target dies (DEAD, respawn timer started, intent cleared) and a DeathEvent
// is recorded. Returns true if the hit killed.
bool apply_damage(WorldState& state, const GameConfig& config, Unit& target, std::uint32_t amount,
                  std::uint32_t attacker_id);
}  // namespace lit::game
