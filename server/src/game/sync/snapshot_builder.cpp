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

// An event reaches a recipient only when every unit it names is visible to it:
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

::game::v1::ServerMessage build_snapshot(const WorldState& state, const ClientSession& recipient,
                                         const Vision& vision, bool resync) {
    const Territory& territory = state.territory;
    const Vision nothing;
    const Vision& told = resync ? nothing : recipient.sync.vision;  // what the client knows
    ::game::v1::ServerMessage msg;
    auto* snap = msg.mutable_snapshot();
    snap->set_tick(state.tick);
    snap->set_resync(resync);

    const std::uint32_t self = recipient.character_id;
    auto* you = snap->mutable_you();
    you->set_last_input_seq(recipient.input.last_input_seq);
    if (const Unit* body = find_unit(state, self)) {
        you->set_life(body->life);
        you->set_respawn_tick(body->respawn_tick);
        you->set_attack_ready_tick(body->attack_ready_tick);
        you->set_attack_cooldown_ticks(body->cooldown_ticks);
        you->set_block_ready_tick(body->block_ready_tick);
        you->set_block_cooldown_ticks(body->block_cooldown_ticks);
    } else {
        you->set_life(::game::v1::LIFE_STATE_NOT_SPAWNED);
    }

    // Alive units and dead bodies are both shown.
    std::unordered_set<std::uint32_t> visible{self};  // units this recipient sees
    for (const auto& [id, u] : state.units) {
        if (id != self && !sees_point(vision, territory, u.x, u.y)) {
            continue;  // in the fog; the recipient's own body is always shown
        }
        visible.insert(id);
        auto* ps = snap->add_players();
        ps->set_id(id);
        ps->set_x(static_cast<std::uint32_t>(u.x));
        ps->set_y(static_cast<std::uint32_t>(u.y));
        ps->set_hp(u.hp);  // 0 for a dead body
    }

    // Cells entering / leaving sight since what the recipient was last told; a
    // revealed cell comes with its current state, whatever changed in the fog.
    const auto cell_count = static_cast<std::uint32_t>(territory.owners.size());
    for (std::uint32_t index = 0; index < cell_count; ++index) {
        const bool now = vision.sees(index);
        const bool before = told.sees(index);
        if (now && !before) {
            snap->add_revealed(index);
            fill_cell_update(snap->add_cells(), territory, index);
        } else if (!now && before) {
            snap->add_hidden(index);
        }
    }
    for (std::uint32_t index : territory.dirty) {
        if (vision.sees(index) && told.sees(index)) {  // revealed ones are sent
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
        ps->set_mine(p.owner_id == self);  // whether it is yours, never whose
    }
    return msg;
}
}  // namespace lit::game
