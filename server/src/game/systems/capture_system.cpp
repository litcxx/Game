#include "systems/capture_system.hpp"

#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace lit::game {
namespace {
// The progress sums to 100 a hair short when 100 / ticks rounds down (120 steps
// of 100 / 120 make 99.99999999999987): far below any step.
constexpr double kCaptured = 100.0 - 1e-9;

// Whether a side neighbour of the cell, on the map (a row's ends don't wrap), is
// `faction`'s.
bool borders(const Territory& t, std::uint32_t index, std::uint32_t faction) {
    const std::uint32_t col = index % t.width;
    const std::uint32_t row = index / t.width;
    const auto ours = [&t, faction](std::uint32_t i) { return t.owners[i] == faction; };
    return (col > 0 && ours(index - 1)) || (col + 1 < t.width && ours(index + 1)) ||
           (row > 0 && ours(index - t.width)) || (row + 1 < t.height && ours(index + t.width));
}

// Progress per tick, in percent; no ticks: at once.
double step(const GameConfig& config, bool enemy) {
    const double ticks = config.capture_ticks * (enemy ? config.capture_enemy_multiplier : 1.0);
    return ticks <= 0.0 ? 100.0 : 100.0 / ticks;
}
}  // namespace

bool capturable(const Territory& territory, std::uint32_t index, std::uint32_t faction) {
    return territory.owners[index] != faction && !territory.protected_cells[index] &&
           borders(territory, index, faction);
}

std::uint32_t update_captures(WorldState& state, const GameConfig& config) {
    Territory& t = state.territory;

    // One faction per cell; mixed factions -> contested (0) -> no progress.
    std::unordered_map<std::uint32_t, std::uint32_t> claim;  // cell index -> faction
    for (const auto& [id, u] : state.units) {
        if (u.life != ::game::v1::LIFE_STATE_ALIVE || !u.intent.capturing) {
            continue;
        }
        const std::uint32_t index = t.index_at(u.x, u.y);
        auto [it, inserted] = claim.try_emplace(index, u.faction_id);
        if (!inserted && it->second != u.faction_id) it->second = 0;  // contested
    }

    std::unordered_set<std::uint32_t> still_active;
    std::uint32_t captured = 0;

    for (const auto& [index, faction] : claim) {
        if (faction == 0 || !capturable(t, index, faction)) {
            continue;  // contested, or not this faction's to take
        }
        if (t.capture_faction[index] != faction) {  // a different faction takes over
            t.capture_faction[index] = static_cast<std::uint8_t>(faction);
            t.capture_progress[index] = 0.0;
        }
        t.capture_progress[index] += step(config, /*enemy=*/t.owners[index] != 0);
        t.dirty.insert(index);
        if (t.capture_progress[index] >= kCaptured) {
            t.set_owner(index, static_cast<std::uint8_t>(faction));
            t.capture_faction[index] = 0;
            t.capture_progress[index] = 0.0;
            ++captured;
        } else {
            still_active.insert(index);
        }
    }

    // Reset any in-progress cell whose capturer stopped (or may no longer take it).
    for (std::uint32_t index : t.active) {
        if (still_active.count(index) != 0) continue;
        if (t.capture_progress[index] != 0.0 || t.capture_faction[index] != 0) {
            t.capture_progress[index] = 0.0;
            t.capture_faction[index] = 0;
            t.dirty.insert(index);
        }
    }
    t.active = std::move(still_active);
    return captured;
}
}  // namespace lit::game
