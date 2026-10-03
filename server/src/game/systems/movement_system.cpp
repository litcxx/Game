#include "systems/movement_system.hpp"

#include <algorithm>
#include <cmath>

#include "state/map_scale.hpp"

namespace lit::game {
Position step_position(Position from, std::int32_t move_x, std::int32_t move_y, double speed,
                       double dt, double max_x, double max_y) {
    if (move_x == 0 && move_y == 0) return from;  // standing still
    const double mx = static_cast<double>(move_x);
    const double my = static_cast<double>(move_y);
    const double len = std::sqrt(mx * mx + my * my);
    return {std::clamp(from.x + (mx / len) * speed * dt, 0.0, max_x),
            std::clamp(from.y + (my / len) * speed * dt, 0.0, max_y)};
}

void integrate_movement(WorldState& state, const GameConfig& config, double dt) {
    const double max_x = static_cast<double>(config.map_width) * kUnitsPerCell - 1.0;
    const double max_y = static_cast<double>(config.map_height) * kUnitsPerCell - 1.0;
    const double speed = static_cast<double>(config.move_speed);

    for (auto& [id, u] : state.units) {
        if (u.life != ::game::v1::LIFE_STATE_ALIVE) continue;
        const Position to =
            step_position({u.x, u.y}, u.intent.move_x, u.intent.move_y, speed, dt, max_x, max_y);
        u.x = to.x;
        u.y = to.y;
    }
}
}  // namespace lit::game
