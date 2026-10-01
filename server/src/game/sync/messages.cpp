#include "sync/messages.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
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

void fill_player_info(::game::v1::PlayerInfo* info, const WorldState& state,
                      const Character& character) {
    info->set_id(character.id);
    info->set_name(character.name);
    info->set_faction_id(faction_of(state, character.id));
}
}  // namespace

::game::v1::ServerMessage make_welcome(const WorldState& state, const GameConfig& config,
                                       const Character& character, std::string_view session_token,
                                       bool resumed) {
    ::game::v1::ServerMessage msg;
    auto* welcome = msg.mutable_welcome();
    welcome->set_player_id(character.id);
    welcome->set_session_token(std::string{session_token});
    welcome->set_resumed(resumed);
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
    for (const auto& c : config.capitals) {  // static: seen through the fog
        auto* capital = welcome->mutable_map()->add_capitals();
        capital->set_faction_id(c.faction_id);
        capital->set_cell(c.cell);
        capital->set_protected_radius(c.protected_radius);
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
    map->set_owner_faction_ids(std::move(owners));
    for (std::uint32_t index : t.active) {
        if (vision.sees(index)) fill_cell_update(map->add_captures(), t, index);
    }
    return msg;
}

::game::v1::ServerMessage make_full_roster(const WorldState& state) {
    ::game::v1::ServerMessage msg;
    auto* roster = msg.mutable_roster();
    roster->set_full(true);
    for (const auto& [id, character] : state.characters) {
        if (character.in_world) fill_player_info(roster->add_upsert(), state, character);
    }
    return msg;
}

::game::v1::ServerMessage make_roster_upsert(const WorldState& state, const Character& character) {
    ::game::v1::ServerMessage msg;
    fill_player_info(msg.mutable_roster()->add_upsert(), state, character);
    return msg;
}

::game::v1::ServerMessage make_roster_removed(std::uint32_t player_id) {
    ::game::v1::ServerMessage msg;
    msg.mutable_roster()->add_removed_player_ids(player_id);
    return msg;
}

::game::v1::ServerMessage make_pong(const WorldState& state, std::uint32_t client_time_ms) {
    ::game::v1::ServerMessage msg;
    auto* pong = msg.mutable_pong();
    pong->set_client_time_ms(client_time_ms);
    pong->set_server_tick(state.tick);
    return msg;
}

bool is_fatal(::game::v1::ErrorCode code) {
    return code > ::game::v1::ERROR_CODE_UNSPECIFIED && code < 20;
}

::game::v1::ServerMessage make_error(::game::v1::ErrorCode code, std::uint32_t request_id,
                                     std::string_view detail) {
    ::game::v1::ServerMessage msg;
    auto* error = msg.mutable_error();
    error->set_code(code);
    error->set_fatal(is_fatal(code));
    error->set_request_id(request_id);
    error->set_detail(std::string{detail});
    return msg;
}

void fill_cell_update(::game::v1::CellUpdate* out, const Territory& territory,
                      std::uint32_t index) {
    out->set_index(index);
    out->set_owner_faction_id(territory.owners[index]);
    out->set_capture_faction_id(territory.capture_faction[index]);
    out->set_capture_progress(static_cast<std::uint32_t>(territory.capture_progress[index]));
}

::game::v1::ServerMessage make_faction_scores(const WorldState& state, const GameConfig& config) {
    std::unordered_map<std::uint32_t, std::uint32_t> online;  // faction id -> in the world
    for (const auto& [id, character] : state.characters) {
        if (character.in_world && character.faction_id != 0) ++online[character.faction_id];
    }
    ::game::v1::ServerMessage msg;
    auto* scores = msg.mutable_faction_scores();
    for (const FactionConfig& faction : config.factions) {
        auto* score = scores->add_scores();
        score->set_faction_id(faction.id);
        score->set_cells(state.territory.owned(faction.id));
        score->set_online(online[faction.id]);
    }
    return msg;
}
}  // namespace lit::game
