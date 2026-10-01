#pragma once

#include <cstdint>

#include "config/config.hpp"
#include "state/territory.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// Territory capture, once per tick. Each alive unit holding capture claims the
// cell under its centre; a cell claimed by one faction (mixed factions contest
// it) that the faction may take (capturable) gains progress and flips to that
// faction at 100: a neutral cell in capture_ticks, an enemy one in capture_ticks
// × capture_enemy_multiplier. A cell whose capturer stopped, or may no longer
// take it, resets. Every changed cell is marked dirty. Returns how many cells
// changed owner this tick.
std::uint32_t update_captures(WorldState& state, const GameConfig& config);

// Whether `faction` (>= 1) may take the cell `index` (GDD 7.3): not its own, not
// in a capital's zone (protected), and next to one of its cells by a side.
bool capturable(const Territory& territory, std::uint32_t index, std::uint32_t faction);
}  // namespace lit::game
