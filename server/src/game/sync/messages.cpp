#include "sync/messages.hpp"

#include <string>
#include <utility>

namespace lit::game {
namespace {
::game::v1::AbilityKind to_wire(AbilityKind kind) {
    switch (kind) {
        case AbilityKind::Melee:
            return ::game::v1::ABILITY_KIND_MELEE;
        case AbilityKind::Projectile:
            return ::game::v1::ABILITY_KIND_PROJECTILE;
        case AbilityKind::Block:
            return ::game::v1::ABILITY_KIND_BLOCK;
    }
    return ::game::v1::ABILITY_KIND_UNSPECIFIED;
}

void fill_player_info(::game::v1::PlayerInfo* info, const Player& player) {
    info->set_id(player.id);
    info->set_name(player.name);
    info->set_faction_id(player.faction_id);
}
}  // namespace

::game::v1::ServerMessage make_welcome(const WorldState& state, const GameConfig& config,
                                       const Player& player) {
    ::game::v1::ServerMessage msg;
    auto* welcome = msg.mutable_welcome();
    welcome->set_player_id(player.id);
    welcome->set_server_tick(state.tick);

    auto* cfg = welcome->mutable_config();
    cfg->set_tick_rate(config.tick_rate);
    cfg->set_snapshot_rate(config.snapshot_rate);
    cfg->set_map_width(config.map_width);
    cfg->set_map_height(config.map_height);
    cfg->set_move_speed(config.move_speed);
    cfg->set_max_hp(config.max_hp);
    cfg->set_respawn_delay_ticks(config.respawn_delay_ticks);
    cfg->set_reconnect_grace_ms(config.reconnect_grace_ms);
    cfg->set_player_radius(config.player_radius);

    for (const auto& f : config.factions) {
        auto* faction = welcome->add_factions();
        faction->set_id(f.id);
        faction->set_name(f.name);
        faction->set_color(f.color);
    }
    for (const auto& a : config.abilities) {
        auto* ability = welcome->add_abilities();
        ability->set_id(a.id);
        ability->set_kind(to_wire(a.kind));
        ability->set_name(a.name);
        ability->set_cooldown_ticks(a.cooldown_ticks);
        ability->set_damage(a.damage);
        ability->set_range(a.range);
        ability->set_projectile_speed(a.projectile_speed);
        ability->set_projectile_radius(a.projectile_radius);
        ability->set_duration_ticks(a.duration_ticks);
    }
    return msg;
}

::game::v1::ServerMessage make_map_state(const WorldState& state, const Vision& vision) {
    const Territory& t = state.territory;
    ::game::v1::ServerMessage msg;
    auto* map = msg.mutable_map_state();
    map->set_tick(state.tick);
    std::string owners(t.owners.size(), '\0');
    for (std::uint32_t index = 0; index < owners.size(); ++index) {
        if (vision.sees(index)) owners[index] = static_cast<char>(t.owners[index]);
    }
    map->set_owners(std::move(owners));
    for (std::uint32_t index : t.active) {
        if (vision.sees(index)) fill_cell_update(map->add_captures(), t, index);
    }
    return msg;
}

::game::v1::ServerMessage make_full_roster(const WorldState& state) {
    ::game::v1::ServerMessage msg;
    auto* roster = msg.mutable_roster();
    roster->set_full(true);
    for (const auto& [session_id, player] : state.players) {
        fill_player_info(roster->add_upsert(), player);
    }
    return msg;
}

::game::v1::ServerMessage make_roster_upsert(const Player& player) {
    ::game::v1::ServerMessage msg;
    fill_player_info(msg.mutable_roster()->add_upsert(), player);
    return msg;
}

::game::v1::ServerMessage make_roster_removed(std::uint32_t player_id) {
    ::game::v1::ServerMessage msg;
    msg.mutable_roster()->add_removed(player_id);
    return msg;
}

::game::v1::ServerMessage make_pong(const WorldState& state, std::uint32_t client_time_ms) {
    ::game::v1::ServerMessage msg;
    auto* pong = msg.mutable_pong();
    pong->set_client_time_ms(client_time_ms);
    pong->set_server_tick(state.tick);
    return msg;
}

void fill_cell_update(::game::v1::CellUpdate* out, const Territory& territory,
                      std::uint32_t index) {
    out->set_index(index);
    out->set_owner(territory.owners[index]);
    out->set_capture_faction(territory.capture_faction[index]);
    out->set_capture_progress(static_cast<std::uint32_t>(territory.capture_progress[index]));
}
}  // namespace lit::game
