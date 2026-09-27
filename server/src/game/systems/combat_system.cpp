#include "systems/combat_system.hpp"

#include <cstdint>

#include "systems/damage.hpp"

namespace lit::game {
namespace {
// The ability a player selected: 0 = the first (default) one; nullptr if unknown.
const AbilityConfig* find_ability(const GameConfig& config, std::uint32_t id) {
    if (config.abilities.empty()) return nullptr;
    if (id == 0) return &config.abilities.front();
    for (const auto& ability : config.abilities) {
        if (ability.id == id) return &ability;
    }
    return nullptr;
}

// Area hit: every enemy within the ability's range of the attacker is struck
// (apply_damage skips anyone already killed earlier this tick).
void melee_strike(WorldState& state, const GameConfig& config, const SpatialIndex& index,
                  std::uint64_t self_sid, const Player& self, const AbilityConfig& ability) {
    const double range = static_cast<double>(ability.range);
    index.for_each_in_radius(self.x, self.y, range, [&](std::uint64_t target_sid) {
        if (target_sid == self_sid) return;
        auto it = state.players.find(target_sid);
        if (it == state.players.end()) return;
        Player& target = it->second;
        if (target.faction_id == self.faction_id) return;  // no friendly fire
        apply_damage(state, config, target, ability.damage, self.id);
    });
}
}  // namespace

void resolve_attacks(WorldState& state, const GameConfig& config, const SpatialIndex& index) {
    for (auto& [attacker_sid, attacker] : state.players) {
        if (attacker.life != ::game::v1::LIFE_STATE_ALIVE) continue;
        if (!attacker.attack) continue;                         // attack key not held
        if (state.tick < attacker.attack_ready_tick) continue;  // on cooldown
        const AbilityConfig* ability = find_ability(config, attacker.ability);
        if (ability == nullptr) continue;  // unknown ability: nothing happens

        switch (ability->kind) {
            case AbilityKind::Melee:
                melee_strike(state, config, index, attacker_sid, attacker, *ability);
                break;
            case AbilityKind::Projectile:
                continue;  // not implemented yet: nothing happens, cooldown untouched
        }
        // Used: the shared cooldown starts, with this ability's length.
        attacker.attack_ready_tick = state.tick + ability->cooldown_ticks;
        attacker.cooldown_ticks = ability->cooldown_ticks;
    }
}
}  // namespace lit::game
