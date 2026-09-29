#include "systems/movement_system.hpp"

#include <algorithm>
#include <cmath>

#include "state/map_scale.hpp"

namespace lit::game {
void integrate_movement(WorldState& state, const GameConfig& config, double dt) {
    const double bound_x = static_cast<double>(config.map_width) * kUnitsPerCell;
    const double bound_y = static_cast<double>(config.map_height) * kUnitsPerCell;

    for (auto& [id, u] : state.units) {
        if (u.life != ::game::v1::LIFE_STATE_ALIVE) {
            continue;
        }
        if (u.intent.move_x == 0 && u.intent.move_y == 0) {
            continue;  // standing still
        }
        const double mx = static_cast<double>(u.intent.move_x);
        const double my = static_cast<double>(u.intent.move_y);
        const double len = std::sqrt(mx * mx + my * my);
        if (len <= 0.0) {
            continue;
        }
        u.x += (mx / len) * config.move_speed * dt;
        u.y += (my / len) * config.move_speed * dt;
        u.x = std::clamp(u.x, 0.0, bound_x - 1.0);
        u.y = std::clamp(u.y, 0.0, bound_y - 1.0);
    }
}
}  // namespace lit::game
