#pragma once

#include "config/config.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// Territory capture, once per tick. Each alive player holding capture claims the
// cell under its center; a cell claimed by one faction (mixed factions contest
// it) gains 100/capture_ticks progress and flips to that faction at 100. A cell
// whose capturer stopped resets. Every changed cell is marked dirty.
void update_captures(WorldState& state, const GameConfig& config);
}  // namespace lit::game
