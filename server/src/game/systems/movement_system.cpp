#include "systems/movement_system.hpp"

#include <algorithm>
#include <cmath>

#include "state/units.hpp"

namespace lit::game {
void integrate_movement(WorldState& state, const GameConfig& config, double dt) {
    const double bound_x = static_cast<double>(config.map_width) * kUnitsPerCell;
    const double bound_y = static_cast<double>(config.map_height) * kUnitsPerCell;

    for (auto& [session_id, p] : state.players) {
        if (p.life != ::game::v1::LIFE_STATE_ALIVE) {
            continue;
        }
        if (p.move_x == 0 && p.move_y == 0) {
            continue;  // standing still
        }
        const double mx = static_cast<double>(p.move_x);
        const double my = static_cast<double>(p.move_y);
        const double len = std::sqrt(mx * mx + my * my);
        if (len <= 0.0) {
            continue;
        }
        p.x += (mx / len) * config.move_speed * dt;
        p.y += (my / len) * config.move_speed * dt;
        p.x = std::clamp(p.x, 0.0, bound_x - 1.0);
        p.y = std::clamp(p.y, 0.0, bound_y - 1.0);
    }
}
}  // namespace lit::game
