#pragma once

#include "config/config.hpp"
#include "spatial/spatial_index.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// Attacks, once per tick. An alive unit holding attack whose shared cooldown is
// ready uses the selected ability (Intent.ability; 0 = the first, unknown or a
// block = nothing here): melee hits every enemy (another faction) within the
// ability's range through apply_damage; a projectile ability launches a shot
// along the aim. Using an ability starts the shared cooldown with that ability's
// length — even when nothing is hit — and is announced with an AbilityEvent.
// `index` holds this tick's alive units (by id) after movement.
void resolve_attacks(WorldState& state, const GameConfig& config, const SpatialIndex& index);

// Blocks, once per tick, before attacks (so a block pressed in the same tick as
// a swing already stops it). An alive unit holding attack with a block ability
// selected starts a block when its own block timer is ready: no damage
// for the ability's duration_ticks, then that ability's cooldown. Independent of
// the shared attack cooldown in both directions.
void activate_blocks(WorldState& state, const GameConfig& config);
}  // namespace lit::game
