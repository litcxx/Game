#pragma once

#include "config/config.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// Move every alive unit along its normalized intent at move_speed for dt
// seconds, clamped to the map.
void integrate_movement(WorldState& state, const GameConfig& config, double dt);
}  // namespace lit::game
