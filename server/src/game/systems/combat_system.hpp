#pragma once

#include "config/config.hpp"
#include "spatial/spatial_index.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// Attacks, once per tick. An alive player holding attack whose shared cooldown is
// ready uses the selected ability (InputFrame.ability; 0 = the first, unknown =
// nothing): melee hits every enemy (another faction) within the ability's range
// through apply_damage. Using an ability starts the shared cooldown with that
// ability's length — even when nothing is hit. `index` holds this tick's alive
// players (session id keys) after movement.
void resolve_attacks(WorldState& state, const GameConfig& config, const SpatialIndex& index);
}  // namespace lit::game
