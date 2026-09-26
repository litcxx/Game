#include "world.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

namespace lit::game {
namespace {
constexpr std::uint32_t kUnitsPerCell = 100;  // matches protocol UNITS_PER_CELL

void fill_player_info(::game::v1::PlayerInfo* info, const Player& player) {
    info->set_id(player.id);
    info->set_name(player.name);
    info->set_faction_id(player.faction_id);
}
}  // namespace

World::World(TSQueue<ClientEvent>& incoming, IClientGateway& gateway, const GameConfig& config)
    : incoming_{incoming}, gateway_{gateway}, config_{config} {
    owners_.assign(static_cast<std::size_t>(config_.map_width) * config_.map_height, 0);

    const std::uint32_t rate = config_.snapshot_rate == 0 ? 1 : config_.snapshot_rate;
    snapshot_interval_ = config_.tick_rate / rate;
    if (snapshot_interval_ == 0) snapshot_interval_ = 1;
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

        std::this_thread::sleep_until(now + tick_duration);
    }
}

void World::tick(double dt) {
    ++tick_;

    // Move the whole inbound queue into a tick-local one in one shot.
    swap(incoming_, local_);

    while (!local_.empty()) {
        auto ev = local_.try_pop();
        if (ev) {
            process_event(ev.value());
        }
    }

    update(dt);
    send_snapshots();
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
    auto it = players_.find(session_id);
    if (it == players_.end()) {
        return;  // never sent Hello
    }
    Player& player = it->second;

    if (player.life != ::game::v1::LIFE_STATE_NOT_SPAWNED) {
        spdlog::warn("World::on_spawn ignored (already spawned) session={}", session_id);
        return;  // respawn from DEAD comes in M3
    }
    if (spawn.cell() >= config_.map_width * config_.map_height) {
        spdlog::warn("World::on_spawn invalid cell={} session={}", spawn.cell(), session_id);
        return;
    }
    bool valid_faction = false;
    for (const auto& f : config_.factions) {
        if (f.id == spawn.faction_id()) valid_faction = true;
    }
    if (!valid_faction) {
        spdlog::warn("World::on_spawn invalid faction={} session={}", spawn.faction_id(),
                     session_id);
        return;
    }

    const std::uint32_t col = spawn.cell() % config_.map_width;
    const std::uint32_t row = spawn.cell() / config_.map_width;
    player.faction_id = spawn.faction_id();
    player.x = col * kUnitsPerCell + kUnitsPerCell / 2.0;
    player.y = row * kUnitsPerCell + kUnitsPerCell / 2.0;
    player.hp = config_.max_hp;
    player.life = ::game::v1::LIFE_STATE_ALIVE;
    player.move_x = 0;
    player.move_y = 0;

    // Faction (colour) chosen -> tell everyone else.
    broadcast_except(session_id, make_roster_upsert(player));
    spdlog::info("World::on_spawn session={} player_id={} cell={} faction={}", session_id,
                 player.id, spawn.cell(), player.faction_id);
}

void World::on_input(std::uint64_t session_id, const ::game::v1::Input& input) {
    auto it = players_.find(session_id);
    if (it == players_.end()) {
        return;
    }
    Player& player = it->second;
    if (player.life != ::game::v1::LIFE_STATE_ALIVE) {
        return;  // only alive players act
    }
    if (input.frames_size() == 0) {
        return;
    }

    // Latest frame wins as the current movement intent; update() integrates it.
    const auto& frame = input.frames(input.frames_size() - 1);
    player.move_x = frame.move_x();
    player.move_y = frame.move_y();
    player.last_input_seq = frame.seq();
}

void World::update(double dt) {
    const double bound_x = static_cast<double>(config_.map_width) * kUnitsPerCell;
    const double bound_y = static_cast<double>(config_.map_height) * kUnitsPerCell;

    for (auto& [session_id, p] : players_) {
        if (p.life != ::game::v1::LIFE_STATE_ALIVE) {
            continue;
        }
        if (p.move_x == 0 && p.move_y == 0) {
            continue;  // standing still
        }
        const double mx = static_cast<double>(p.move_x);
        const double my = static_cast<double>(p.move_y);
        const double len = std::sqrt(mx * mx + my * my);
        if (len <= 0.0) {
            continue;
        }
        p.x += (mx / len) * config_.move_speed * dt;
        p.y += (my / len) * config_.move_speed * dt;
        p.x = std::clamp(p.x, 0.0, bound_x - 1.0);
        p.y = std::clamp(p.y, 0.0, bound_y - 1.0);
    }
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
    if (snapshot_interval_ == 0 || tick_ % snapshot_interval_ != 0) {
        return;
    }

    // Every connected player gets a Snapshot (they need to see the world even
    // before spawning); `players` lists everyone alive, `you` is per-recipient.
    for (const auto& [session_id, self] : players_) {
        ::game::v1::ServerMessage msg;
        auto* snap = msg.mutable_snapshot();
        snap->set_tick(tick_);

        auto* you = snap->mutable_you();
        you->set_life(self.life);
        you->set_last_input_seq(self.last_input_seq);

        for (const auto& [sid, p] : players_) {
            if (p.life != ::game::v1::LIFE_STATE_ALIVE) {
                continue;
            }
            auto* ps = snap->add_players();
            ps->set_id(p.id);
            ps->set_x(static_cast<std::uint32_t>(p.x));
            ps->set_y(static_cast<std::uint32_t>(p.y));
            ps->set_hp(p.hp);
        }
        send(session_id, msg);
    }
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
