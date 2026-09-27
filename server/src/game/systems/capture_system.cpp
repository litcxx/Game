#include "systems/capture_system.hpp"

#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace lit::game {
void update_captures(WorldState& state, const GameConfig& config) {
    Territory& t = state.territory;

    // One faction per cell; mixed factions -> contested (0) -> no progress.
    std::unordered_map<std::uint32_t, std::uint32_t> claim;  // cell index -> faction
    for (const auto& [session_id, p] : state.players) {
        if (p.life != ::game::v1::LIFE_STATE_ALIVE || !p.capturing) {
            continue;
        }
        const std::uint32_t index = t.index_at(p.x, p.y);
        auto [it, inserted] = claim.try_emplace(index, p.faction_id);
        if (!inserted && it->second != p.faction_id) it->second = 0;  // contested
    }

    const double step = config.capture_ticks == 0 ? 100.0 : 100.0 / config.capture_ticks;
    std::unordered_set<std::uint32_t> still_active;

    for (const auto& [index, faction] : claim) {
        if (faction == 0 || t.owners[index] == faction) {
            continue;  // contested, or already ours
        }
        if (t.capture_faction[index] != faction) {  // a different faction takes over
            t.capture_faction[index] = static_cast<std::uint8_t>(faction);
            t.capture_progress[index] = 0.0;
        }
        t.capture_progress[index] += step;
        t.dirty.insert(index);
        if (t.capture_progress[index] >= 100.0) {
            t.owners[index] = static_cast<std::uint8_t>(faction);
            t.capture_faction[index] = 0;
            t.capture_progress[index] = 0.0;
        } else {
            still_active.insert(index);
        }
    }

    // Reset any in-progress cell whose capturer stopped this tick.
    for (std::uint32_t index : t.active) {
        if (still_active.count(index) != 0) continue;
        if (t.capture_progress[index] != 0.0 || t.capture_faction[index] != 0) {
            t.capture_progress[index] = 0.0;
            t.capture_faction[index] = 0;
            t.dirty.insert(index);
        }
    }
    t.active = std::move(still_active);
}
}  // namespace lit::game
