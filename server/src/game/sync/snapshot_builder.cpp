#include "sync/snapshot_builder.hpp"

#include <cstdint>

#include "sync/messages.hpp"

namespace lit::game {
::game::v1::ServerMessage build_snapshot(const WorldState& state, const Player& recipient) {
    ::game::v1::ServerMessage msg;
    auto* snap = msg.mutable_snapshot();
    snap->set_tick(state.tick);

    auto* you = snap->mutable_you();
    you->set_life(recipient.life);
    you->set_last_input_seq(recipient.last_input_seq);
    you->set_respawn_tick(recipient.respawn_tick);
    you->set_attack_ready_tick(recipient.attack_ready_tick);
    you->set_attack_cooldown_ticks(recipient.cooldown_ticks);

    for (const auto& [session_id, p] : state.players) {
        if (p.life == ::game::v1::LIFE_STATE_NOT_SPAWNED) {
            continue;  // no body yet; alive players and dead bodies are both shown
        }
        auto* ps = snap->add_players();
        ps->set_id(p.id);
        ps->set_x(static_cast<std::uint32_t>(p.x));
        ps->set_y(static_cast<std::uint32_t>(p.y));
        ps->set_hp(p.hp);  // 0 for a dead body
    }
    for (std::uint32_t index : state.territory.dirty) {
        fill_cell_update(snap->add_cells(), state.territory, index);
    }
    for (const auto& ev : state.events) {
        *snap->add_events() = ev;
    }
    return msg;
}
}  // namespace lit::game
