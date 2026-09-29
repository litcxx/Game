#pragma once

#include <cstdint>

namespace lit::game {
// A projectile in flight. Owner and faction are captured at launch: a shot
// stays the shooter's (hit credit, no friendly fire) even if they die, leave
// or respawn in another faction.
struct Projectile {
    std::uint32_t id{0};
    std::uint32_t owner_id{0};    // shooter's unit id (hit / kill credit)
    std::uint32_t faction_id{0};  // shooter's faction at launch
    std::uint32_t damage{0};
    double radius{0.0};  // units
    double x{0.0};       // position, units
    double y{0.0};
    double vx{0.0};  // velocity, units per second
    double vy{0.0};
    double remaining{0.0};  // flight distance left, units
};
}  // namespace lit::game
