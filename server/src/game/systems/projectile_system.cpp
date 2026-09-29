#include "systems/projectile_system.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "spatial/geometry.hpp"
#include "state/map_scale.hpp"
#include "systems/damage.hpp"

namespace lit::game {
void update_projectiles(WorldState& state, const GameConfig& config, const SpatialIndex& index,
                        double dt) {
    const double width = static_cast<double>(config.map_width) * kUnitsPerCell;
    const double height = static_cast<double>(config.map_height) * kUnitsPerCell;
    const double body = static_cast<double>(config.player_radius);

    for (Projectile& p : state.projectiles) {
        // This tick's step, cut short at the end of the range.
        const double speed = std::hypot(p.vx, p.vy);
        const double step = std::min(speed * dt, p.remaining);
        const double scale = speed > 0.0 ? step / speed : 0.0;
        const double x1 = p.x + p.vx * scale;
        const double y1 = p.y + p.vy * scale;

        // The first enemy the step touches: candidates within reach of the segment.
        const double reach = body + p.radius;
        Unit* first = nullptr;
        double first_t = 2.0;
        index.for_each_in_radius(
            (p.x + x1) / 2.0, (p.y + y1) / 2.0, step / 2.0 + reach, [&](std::uint32_t id) {
                Unit* target = find_unit(state, id);
                if (target == nullptr) return;
                // Killed earlier this tick: a body doesn't stop shots.
                if (target->life != ::game::v1::LIFE_STATE_ALIVE) return;
                if (target->id == p.owner_id) return;            // never its shooter
                if (target->faction_id == p.faction_id) return;  // allies
                const auto t = segment_circle_hit(p.x, p.y, x1, y1, target->x, target->y, reach);
                if (t && *t < first_t) {
                    first_t = *t;
                    first = target;
                }
            });
        if (first != nullptr) {
            apply_damage(state, config, *first, p.damage, p.owner_id);
            p.remaining = 0.0;  // spent on the hit
            continue;
        }

        p.x = x1;
        p.y = y1;
        p.remaining -= step;
        if (x1 < 0.0 || y1 < 0.0 || x1 >= width || y1 >= height) p.remaining = 0.0;  // left the map
    }
    std::erase_if(state.projectiles, [](const Projectile& p) { return p.remaining <= 0.0; });
}
}  // namespace lit::game
