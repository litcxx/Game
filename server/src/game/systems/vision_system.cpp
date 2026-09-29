#include "systems/vision_system.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <utility>

#include "state/map_scale.hpp"

namespace lit::game {
namespace {
// The cells [first, last] along one axis whose centres lie within `radius` of
// `v`, clamped onto [0, count); empty (first > last) when there are none.
std::pair<std::int64_t, std::int64_t> cells_in_reach(double v, double radius, std::uint32_t count) {
    const double cell = kUnitsPerCell;
    const auto first = static_cast<std::int64_t>(std::ceil((v - radius) / cell - 0.5));
    const auto last = static_cast<std::int64_t>(std::floor((v + radius) / cell - 0.5));
    return {std::max<std::int64_t>(first, 0),
            std::min<std::int64_t>(last, static_cast<std::int64_t>(count) - 1)};
}

// Mark every cell whose centre is within `radius` of the source at (x, y).
void reveal_around(Vision& vision, const Territory& t, double x, double y, double radius) {
    const auto [col0, col1] = cells_in_reach(x, radius, t.width);
    const auto [row0, row1] = cells_in_reach(y, radius, t.height);
    const double radius_sq = radius * radius;
    for (auto row = row0; row <= row1; ++row) {
        const double dy = (static_cast<double>(row) + 0.5) * kUnitsPerCell - y;
        for (auto col = col0; col <= col1; ++col) {
            const double dx = (static_cast<double>(col) + 0.5) * kUnitsPerCell - x;
            if (dx * dx + dy * dy <= radius_sq) {
                vision.cells[static_cast<std::size_t>(row * t.width + col)] = 1;
            }
        }
    }
}
}  // namespace

Vision compute_vision(const WorldState& state, const GameConfig& config, std::uint32_t faction_id) {
    Vision vision;
    if (faction_id == 0) {
        return vision;  // no faction yet: no sources (neutral cells are nobody's)
    }
    const Territory& t = state.territory;
    const auto radius = static_cast<double>(config.vision_radius);
    vision.cells.assign(t.owners.size(), 0);

    // Deaths not yet reported: state.events holds this snapshot period's events.
    std::unordered_set<std::uint32_t> dying;
    for (const auto& ev : state.events) {
        if (ev.has_death()) dying.insert(ev.death().victim_id());
    }
    for (const auto& [id, u] : state.units) {
        if (u.faction_id != faction_id) continue;
        const bool alive = u.life == ::game::v1::LIFE_STATE_ALIVE;
        if (!alive && !dying.contains(id)) continue;  // a body, its death already told
        reveal_around(vision, t, u.x, u.y, radius);
    }
    for (std::uint32_t index = 0; index < t.owners.size(); ++index) {
        if (t.owners[index] != faction_id) continue;
        const std::uint32_t col = index % t.width;
        const std::uint32_t row = index / t.width;
        // An owned cell sees from its centre.
        reveal_around(vision, t, (col + 0.5) * kUnitsPerCell, (row + 0.5) * kUnitsPerCell, radius);
    }
    return vision;
}
}  // namespace lit::game
