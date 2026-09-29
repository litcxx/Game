#pragma once

#include "config/config.hpp"
#include "spatial/spatial_index.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// Projectile flight, once per tick (after attacks). Each projectile moves
// speed * dt (never past its remaining range); the step is swept against alive
// enemy units — not its owner, not its faction, alive right now — with the
// combined radius player_radius + projectile radius. The first contact along the
// path takes the projectile's damage through apply_damage (credited to the
// owner) and spends the projectile; otherwise it despawns when its range is used
// up or it leaves the map. `index` holds this tick's alive units after movement.
void update_projectiles(WorldState& state, const GameConfig& config, const SpatialIndex& index,
                        double dt);
}  // namespace lit::game
