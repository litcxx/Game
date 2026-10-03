#pragma once

#include <cstdint>
#include <optional>

#include "config/config.hpp"
#include "spatial/spatial_index.hpp"
#include "state/world_state.hpp"

namespace lit::game {
struct Velocity {
    double vx{0.0};  // units per second
    double vy{0.0};
};

// A shot's velocity along an aim (InputFrame.aim_x / aim_y, any length) at
// `speed` units per second; none without an aim.
std::optional<Velocity> aim_velocity(std::int32_t aim_x, std::int32_t aim_y, double speed);

// One tick of a projectile's flight, hits aside: speed * dt along its velocity,
// never past its remaining range. `length` is how far that is.
struct FlightStep {
    double x{0.0};
    double y{0.0};
    double length{0.0};
};
FlightStep flight_step(const Projectile& p, double dt);

// Whether (x, y) is off a map `width` x `height` units: a projectile there is spent.
bool off_map(double x, double y, double width, double height);

// aim_velocity, flight_step and off_map are what the client's prediction repeats,
// to the bit: only + - * / and sqrt, the same in C++ and in JavaScript
// (protocol/sim/projectile.json pins both sides).

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
