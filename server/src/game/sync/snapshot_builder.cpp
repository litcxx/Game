#include "sync/snapshot_builder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>

#include "sync/messages.hpp"

namespace lit::game {
namespace {
// Whether the cell under a world point is visible.
bool sees_point(const Vision& vision, const Territory& territory, double x, double y) {
    return vision.sees(territory.index_at(std::max(0.0, x), std::max(0.0, y)));
}

// An event reaches a recipient only when every player it names is visible to it:
// a hit from the fog tells the victim nothing. Id 0 names nobody.
bool names_only(const ::game::v1::GameEvent& ev, const std::unordered_set<std::uint32_t>& visible) {
    const auto shown = [&visible](std::uint32_t id) { return id == 0 || visible.contains(id); };
    switch (ev.kind_case()) {
        case ::game::v1::GameEvent::kHit:
            return shown(ev.hit().attacker_id()) && shown(ev.hit().target_id());
        case ::game::v1::GameEvent::kDeath:
            return shown(ev.death().victim_id()) && shown(ev.death().killer_id());
        case ::game::v1::GameEvent::kAbility:
            return shown(ev.ability().player_id());
        case ::game::v1::GameEvent::KIND_NOT_SET:
            break;
    }
    return false;
}
}  // namespace

::game::v1::ServerMessage build_snapshot(const WorldState& state, const Player& recipient,
                                         const Vision& vision) {
    const Territory& territory = state.territory;
    ::game::v1::ServerMessage msg;
    auto* snap = msg.mutable_snapshot();
    snap->set_tick(state.tick);

    auto* you = snap->mutable_you();
    you->set_life(recipient.life);
    you->set_last_input_seq(recipient.last_input_seq);
    you->set_respawn_tick(recipient.respawn_tick);
    you->set_attack_ready_tick(recipient.attack_ready_tick);
    you->set_attack_cooldown_ticks(recipient.cooldown_ticks);
    you->set_block_ready_tick(recipient.block_ready_tick);
    you->set_block_cooldown_ticks(recipient.block_cooldown_ticks);

    std::unordered_set<std::uint32_t> visible{recipient.id};  // players this recipient sees
    for (const auto& [session_id, p] : state.players) {
        if (p.life == ::game::v1::LIFE_STATE_NOT_SPAWNED) {
            continue;  // no body yet; alive players and dead bodies are both shown
        }
        if (p.id != recipient.id && !sees_point(vision, territory, p.x, p.y)) {
            continue;  // in the fog; the recipient itself is always shown
        }
        visible.insert(p.id);
        auto* ps = snap->add_players();
        ps->set_id(p.id);
        ps->set_x(static_cast<std::uint32_t>(p.x));
        ps->set_y(static_cast<std::uint32_t>(p.y));
        ps->set_hp(p.hp);  // 0 for a dead body
    }

    // Cells entering / leaving sight since what the recipient was last told; a
    // revealed cell comes with its current state, whatever changed in the fog.
    const auto cell_count = static_cast<std::uint32_t>(territory.owners.size());
    for (std::uint32_t index = 0; index < cell_count; ++index) {
        const bool now = vision.sees(index);
        const bool before = recipient.vision.sees(index);
        if (now && !before) {
            snap->add_revealed(index);
            fill_cell_update(snap->add_cells(), territory, index);
        } else if (!now && before) {
            snap->add_hidden(index);
        }
    }
    for (std::uint32_t index : territory.dirty) {
        if (vision.sees(index) && recipient.vision.sees(index)) {  // revealed ones are sent
            fill_cell_update(snap->add_cells(), territory, index);
        }
    }

    for (const auto& ev : state.events) {
        if (names_only(ev, visible)) *snap->add_events() = ev;
    }
    for (const auto& p : state.projectiles) {
        if (!sees_point(vision, territory, p.x, p.y)) continue;
        auto* ps = snap->add_projectiles();
        ps->set_id(p.id);
        ps->set_x(static_cast<std::uint32_t>(std::max(0.0, p.x)));
        ps->set_y(static_cast<std::uint32_t>(std::max(0.0, p.y)));
        ps->set_faction_id(p.faction_id);
        ps->set_vx(static_cast<std::int32_t>(std::lround(p.vx)));
        ps->set_vy(static_cast<std::int32_t>(std::lround(p.vy)));
    }
    return msg;
}
}  // namespace lit::game
