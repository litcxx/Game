#include "systems/combat_system.hpp"

#include <cstdint>

#include "systems/damage.hpp"

namespace lit::game {
void resolve_melee(WorldState& state, const GameConfig& config, const SpatialIndex& index) {
    const double range = static_cast<double>(config.attack_range);

    for (auto& [attacker_sid, attacker] : state.players) {
        if (attacker.life != ::game::v1::LIFE_STATE_ALIVE) continue;
        if (!attacker.attack) continue;                         // attack key not held
        if (state.tick < attacker.attack_ready_tick) continue;  // on cooldown

        // A swing fires: it consumes the cooldown even if nothing is in range.
        attacker.attack_ready_tick = state.tick + config.attack_cooldown_ticks;

        // Area hit: every enemy within attack_range of the attacker is struck
        // (apply_damage skips anyone already killed earlier this tick).
        const std::uint64_t self_sid = attacker_sid;
        const Player& self = attacker;
        index.for_each_in_radius(self.x, self.y, range, [&](std::uint64_t target_sid) {
            if (target_sid == self_sid) return;
            auto it = state.players.find(target_sid);
            if (it == state.players.end()) return;
            Player& target = it->second;
            if (target.faction_id == self.faction_id) return;  // no friendly fire
            apply_damage(state, config, target, config.attack_damage, self.id);
        });
    }
}
}  // namespace lit::game
