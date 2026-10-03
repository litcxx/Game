#include "systems/combat_system.hpp"

#include <cstdint>
#include <optional>

#include "systems/damage.hpp"
#include "systems/projectile_system.hpp"

namespace lit::game {
namespace {
// The ability a unit selected: 0 = the first (default) one; nullptr if unknown.
const AbilityConfig* find_ability(const GameConfig& config, std::uint32_t id) {
    if (config.abilities.empty()) return nullptr;
    if (id == 0) return &config.abilities.front();
    for (const auto& ability : config.abilities) {
        if (ability.id == id) return &ability;
    }
    return nullptr;
}

// Area hit: every enemy whose body the ability's range touches is struck — its
// edge, not its centre, as a projectile hits on touching the body (apply_damage
// skips anyone already killed earlier this tick).
void melee_strike(WorldState& state, const GameConfig& config, const SpatialIndex& index,
                  const Unit& self, const AbilityConfig& ability) {
    const double reach =
        static_cast<double>(ability.range) + static_cast<double>(config.player_radius);
    index.for_each_in_radius(self.x, self.y, reach, [&](std::uint32_t target_id) {
        if (target_id == self.id) return;
        Unit* target = find_unit(state, target_id);
        if (target == nullptr) return;
        if (target->faction_id == self.faction_id) return;  // no friendly fire
        apply_damage(state, config, *target, ability.damage, self.id);
    });
}

// Tell every client this unit used the ability (for its effect).
void announce_use(WorldState& state, const Unit& unit, const AbilityConfig& ability) {
    auto& ev = state.events.emplace_back();
    ev.set_tick(state.tick);
    auto* use = ev.mutable_ability();
    use->set_player_id(unit.id);
    use->set_ability_id(ability.id);
}

// Launch a projectile from the unit's centre along its aim. Returns false (and
// launches nothing) when there is no aim.
bool launch_projectile(WorldState& state, const Unit& self, const AbilityConfig& ability) {
    const std::optional<Velocity> v = aim_velocity(self.intent.aim_x, self.intent.aim_y,
                                                   static_cast<double>(ability.projectile_speed));
    if (!v) return false;
    state.projectiles.push_back(
        Projectile{state.next_projectile_id++, self.id, self.faction_id, ability.damage,
                   static_cast<double>(ability.projectile_radius), self.x, self.y, v->vx, v->vy,
                   static_cast<double>(ability.range), self.intent.input_seq});
    return true;
}
}  // namespace

void resolve_attacks(WorldState& state, const GameConfig& config, const SpatialIndex& index) {
    for (auto& [attacker_id, attacker] : state.units) {
        if (attacker.life != ::game::v1::LIFE_STATE_ALIVE) continue;
        if (!attacker.intent.attack) continue;                  // attack not held
        if (state.tick < attacker.attack_ready_tick) continue;  // on cooldown
        const AbilityConfig* ability = find_ability(config, attacker.intent.ability_id);
        if (ability == nullptr) continue;  // unknown ability: nothing happens

        switch (ability->kind) {
            case AbilityKind::Melee:
                melee_strike(state, config, index, attacker, *ability);
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
        attacker.last_attack_input_seq = attacker.intent.input_seq;
        announce_use(state, attacker, *ability);
    }
}

void activate_blocks(WorldState& state, const GameConfig& config) {
    for (auto& [id, unit] : state.units) {
        if (unit.life != ::game::v1::LIFE_STATE_ALIVE) continue;
        if (!unit.intent.attack) continue;                 // attack not held
        if (state.tick < unit.block_ready_tick) continue;  // block on cooldown
        const AbilityConfig* ability = find_ability(config, unit.intent.ability_id);
        if (ability == nullptr || ability->kind != AbilityKind::Block) continue;

        unit.block_until_tick = state.tick + ability->duration_ticks;
        unit.block_ready_tick = state.tick + ability->cooldown_ticks;
        unit.block_cooldown_ticks = ability->cooldown_ticks;
        announce_use(state, unit, *ability);
    }
}
}  // namespace lit::game
