#include "world.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <thread>
#include <vector>

namespace lit::game {
namespace {
void fill_player_info(::game::v1::PlayerInfo* info, const Player& player) {
    info->set_id(player.id);
    info->set_name(player.name);
    info->set_faction_id(player.faction_id);
}
}  // namespace

World::World(TSQueue<ClientEvent>& incoming, IClientGateway& gateway, const GameConfig& config)
    : incoming_{incoming}, gateway_{gateway}, config_{config} {
    owners_.assign(static_cast<std::size_t>(config_.map_width) * config_.map_height, 0);
}

void World::run(std::stop_token stop) {
    const double tick_seconds = 1.0 / config_.tick_rate;
    const auto tick_duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(tick_seconds));

    auto last = std::chrono::steady_clock::now();
    while (!stop.stop_requested()) {
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> delta = now - last;
        last = now;

        tick(delta.count());
        send_snapshots();

        std::this_thread::sleep_until(now + tick_duration);
    }
}

void World::tick(double dt) {
    (void)dt;  // no simulation yet
    ++tick_;

    // Move the whole inbound queue into a tick-local one in one shot.
    swap(incoming_, local_);

    while (!local_.empty()) {
        auto ev = local_.try_pop();
        if (ev) {
            process_event(ev.value());
        }
    }
}

void World::process_event(const ClientEvent& ev) {
    if (ev.kind == ClientEvent::Kind::Disconnected) {
        on_disconnect(ev.session_id);
        return;
    }

    switch (ev.msg.payload_case()) {
        case ::game::v1::ClientMessage::kHello:
            on_hello(ev.session_id, ev.msg.hello());
            break;
        case ::game::v1::ClientMessage::kSpawn:
            on_spawn(ev.session_id, ev.msg.spawn());
            break;
        case ::game::v1::ClientMessage::kInput:
            on_input(ev.session_id, ev.msg.input());
            break;
        case ::game::v1::ClientMessage::kPing:
            on_ping(ev.session_id, ev.msg.ping());
            break;
        case ::game::v1::ClientMessage::PAYLOAD_NOT_SET:
        default:
            spdlog::warn("World: empty/unknown payload from session={}", ev.session_id);
            break;
    }
}

void World::on_hello(std::uint64_t session_id, const ::game::v1::Hello& hello) {
    Player player;
    player.id = next_player_id_++;
    player.name = hello.name();
    players_[session_id] = player;

    // Greet the joiner: Welcome, then the full map, then the full roster.
    send(session_id, make_welcome(player));
    send(session_id, make_map_state());
    send(session_id, make_full_roster());

    // Tell everyone else that a new player joined.
    broadcast_except(session_id, make_roster_upsert(player));

    spdlog::info("World::on_hello session={} name='{}' -> player_id={}", session_id, hello.name(),
                 player.id);
}

void World::on_spawn(std::uint64_t session_id, const ::game::v1::SpawnRequest& spawn) {
    spdlog::info("World::on_spawn session={} cell={} faction_id={}", session_id, spawn.cell(),
                 spawn.faction_id());
}

void World::on_input(std::uint64_t session_id, const ::game::v1::Input& input) {
    spdlog::info("World::on_input session={} frames={}", session_id, input.frames_size());
}

void World::on_ping(std::uint64_t session_id, const ::game::v1::Ping& ping) {
    ::game::v1::ServerMessage reply;
    auto* pong = reply.mutable_pong();
    pong->set_client_time_ms(ping.client_time_ms());
    pong->set_server_tick(tick_);
    send(session_id, reply);
}

void World::on_disconnect(std::uint64_t session_id) {
    auto it = players_.find(session_id);
    if (it == players_.end()) {
        return;  // connected but never sent Hello, or already gone
    }

    const std::uint32_t player_id = it->second.id;
    players_.erase(it);

    // Tell the remaining players the player left (broadcast now excludes it).
    ::game::v1::ServerMessage msg;
    msg.mutable_roster()->add_removed(player_id);
    broadcast(msg);

    spdlog::info("World::on_disconnect session={} player_id={}", session_id, player_id);
}

::game::v1::ServerMessage World::make_welcome(const Player& player) const {
    ::game::v1::ServerMessage msg;
    auto* welcome = msg.mutable_welcome();
    welcome->set_player_id(player.id);
    welcome->set_server_tick(tick_);

    auto* cfg = welcome->mutable_config();
    cfg->set_tick_rate(config_.tick_rate);
    cfg->set_snapshot_rate(config_.snapshot_rate);
    cfg->set_map_width(config_.map_width);
    cfg->set_map_height(config_.map_height);
    cfg->set_move_speed(config_.move_speed);
    cfg->set_max_hp(config_.max_hp);
    cfg->set_attack_range(config_.attack_range);
    cfg->set_attack_cooldown_ticks(config_.attack_cooldown_ticks);
    cfg->set_respawn_delay_ticks(config_.respawn_delay_ticks);
    cfg->set_reconnect_grace_ms(config_.reconnect_grace_ms);

    for (const auto& f : config_.factions) {
        auto* faction = welcome->add_factions();
        faction->set_id(f.id);
        faction->set_name(f.name);
        faction->set_color(f.color);
    }
    return msg;
}

::game::v1::ServerMessage World::make_map_state() const {
    ::game::v1::ServerMessage msg;
    auto* map = msg.mutable_map_state();
    map->set_tick(tick_);
    map->set_owners(owners_.data(), owners_.size());
    return msg;
}

::game::v1::ServerMessage World::make_full_roster() const {
    ::game::v1::ServerMessage msg;
    auto* roster = msg.mutable_roster();
    for (const auto& [session_id, player] : players_) {
        fill_player_info(roster->add_upsert(), player);
    }
    return msg;
}

::game::v1::ServerMessage World::make_roster_upsert(const Player& player) const {
    ::game::v1::ServerMessage msg;
    fill_player_info(msg.mutable_roster()->add_upsert(), player);
    return msg;
}

void World::send_snapshots() {
    // TODO (M2): build per-session Snapshot and deliver at snapshot_rate.
}

void World::send(std::uint64_t session_id, const ::game::v1::ServerMessage& msg) {
    std::vector<std::byte> bytes(msg.ByteSizeLong());
    if (!msg.SerializeToArray(bytes.data(), static_cast<int>(bytes.size()))) {
        spdlog::error("World::send failed to serialize for session={}", session_id);
        return;
    }
    gateway_.send_to(session_id, std::move(bytes));
}

void World::broadcast(const ::game::v1::ServerMessage& msg) {
    for (const auto& [session_id, player] : players_) {
        send(session_id, msg);
    }
}

void World::broadcast_except(std::uint64_t session_id, const ::game::v1::ServerMessage& msg) {
    for (const auto& [sid, player] : players_) {
        if (sid != session_id) {
            send(sid, msg);
        }
    }
}
}  // namespace lit::game
