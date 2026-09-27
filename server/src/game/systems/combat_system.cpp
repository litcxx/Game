#include "systems/combat_system.hpp"

#include <cmath>
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
// Launch a projectile from the player's centre along its aim. Returns false (and
// launches nothing) when there is no aim.
bool launch_projectile(WorldState& state, const Player& self, const AbilityConfig& ability) {
    const double ax = static_cast<double>(self.aim_x);
    const double ay = static_cast<double>(self.aim_y);
    const double len = std::sqrt(ax * ax + ay * ay);
    if (len <= 0.0) return false;
    const double speed = static_cast<double>(ability.projectile_speed);
    state.projectiles.push_back(
        Projectile{state.next_projectile_id++, self.id, self.faction_id, ability.damage,
                   static_cast<double>(ability.projectile_radius), self.x, self.y, ax / len * speed,
                   ay / len * speed, static_cast<double>(ability.range)});
    return true;
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
                if (!launch_projectile(state, attacker, *ability)) continue;  // no aim: no shot
                break;
            case AbilityKind::Block:
                continue;  // not an attack: blocks run on their own timer
        }
        // Used: the shared cooldown starts, with this ability's length.
        attacker.attack_ready_tick = state.tick + ability->cooldown_ticks;
        attacker.cooldown_ticks = ability->cooldown_ticks;
    }
}
}  // namespace lit::game
