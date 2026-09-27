#pragma once

#include "config/config.hpp"
#include "spatial/spatial_index.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// Melee area attack, once per tick. An alive player holding attack whose cooldown
// is ready swings — consuming the cooldown even if nothing is in range — and hits
// every enemy (another faction) within the melee range through apply_damage.
// `index` holds this tick's alive players (session id keys) after movement.
void resolve_melee(WorldState& state, const GameConfig& config, const SpatialIndex& index);
}  // namespace lit::game
