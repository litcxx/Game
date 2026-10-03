#pragma once

#include <cstdint>

#include "config/config.hpp"
#include "state/world_state.hpp"

namespace lit::game {
struct Position {
    double x{0.0};
    double y{0.0};
};

// One step along a direction (any length: normalized; (0, 0) stays put) at
// `speed` units per second for `dt` seconds, clamped to [0, max_x] x [0, max_y].
// The client's prediction repeats it to the bit (protocol/sim/movement.json).
Position step_position(Position from, std::int32_t move_x, std::int32_t move_y, double speed,
                       double dt, double max_x, double max_y);

// Move every alive unit along its normalized intent at move_speed for dt
// seconds, clamped to the map.
void integrate_movement(WorldState& state, const GameConfig& config, double dt);
}  // namespace lit::game
