#include "systems/damage.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace lit::game {
bool apply_damage(WorldState& state, const GameConfig& config, Unit& target, std::uint32_t amount,
                  std::uint32_t attacker_id) {
    if (target.life != ::game::v1::LIFE_STATE_ALIVE) {
        return false;  // a body takes no damage
    }
    if (state.tick < target.block_until_tick) {  // blocking: the hit is stopped
        auto& blocked_ev = state.events.emplace_back();
        blocked_ev.set_tick(state.tick);
        auto* hit = blocked_ev.mutable_hit();
        hit->set_attacker_id(attacker_id);
        hit->set_target_id(target.id);
        hit->set_damage(0);
        hit->set_blocked(true);
        return false;
    }
    const std::uint32_t dmg = std::min(amount, target.hp);
    target.hp -= dmg;

    auto& hit_ev = state.events.emplace_back();
    hit_ev.set_tick(state.tick);
    auto* hit = hit_ev.mutable_hit();
    hit->set_attacker_id(attacker_id);
    hit->set_target_id(target.id);
    hit->set_damage(dmg);

    if (target.hp != 0) {
        return false;
    }
    target.life = ::game::v1::LIFE_STATE_DEAD;
    target.respawn_tick = state.tick + config.respawn_delay_ticks;
    target.intent = Intent{};  // a body does nothing

    auto& death_ev = state.events.emplace_back();
    death_ev.set_tick(state.tick);
    auto* death = death_ev.mutable_death();
    death->set_victim_id(target.id);
    death->set_killer_id(attacker_id);

    spdlog::info("unit {} killed unit {}", attacker_id, target.id);
    return true;
}
}  // namespace lit::game
