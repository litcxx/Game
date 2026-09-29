#pragma once

#include <cstdint>

#include "config/config.hpp"
#include "state/vision.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// Fog of war: the cells `faction_id` sees now. A cell is visible when its centre
// is within config.vision_radius (inclusive) of a vision source — an alive unit
// of the faction or a cell the faction owns. No walls or line-of-sight checks.
// A body stops giving sight once its death is reported: a unit killed since the
// last snapshot (its DeathEvent still in state.events) sees for that one
// snapshot, so the victim is still told who struck it. Faction 0 (none chosen
// yet) has no sources and sees nothing.
Vision compute_vision(const WorldState& state, const GameConfig& config, std::uint32_t faction_id);
}  // namespace lit::game
