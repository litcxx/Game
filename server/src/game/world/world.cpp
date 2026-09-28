#include "world.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <vector>

#include "state/vision.hpp"
#include "sync/delivery.hpp"
#include "sync/messages.hpp"
#include "sync/snapshot_builder.hpp"
#include "systems/capture_system.hpp"
#include "systems/combat_system.hpp"
#include "systems/input_system.hpp"
#include "systems/join_system.hpp"
#include "systems/movement_system.hpp"
#include "systems/projectile_system.hpp"
#include "systems/spawn_system.hpp"
#include "systems/vision_system.hpp"

namespace lit::game {
namespace {
// Spatial-index bucket edge: 2 cells, so a melee query spans at most 3x3 buckets.
constexpr double kIndexBucketUnits = 2.0 * kUnitsPerCell;
}  // namespace

World::World(TSQueue<ClientEvent>& incoming, IClientGateway& gateway, const GameConfig& config)
    : incoming_{incoming},
      gateway_{gateway},
      config_{config},
      alive_index_{static_cast<double>(config.map_width * kUnitsPerCell),
                   static_cast<double>(config.map_height * kUnitsPerCell), kIndexBucketUnits} {
    state_.territory.reset(config_.map_width, config_.map_height);

    const std::uint32_t rate = config_.snapshot_rate == 0 ? 1 : config_.snapshot_rate;
    snapshot_interval_ = config_.tick_rate / rate;
    if (snapshot_interval_ == 0) snapshot_interval_ = 1;

    // Rounded up: a window shorter than a tick still spans one.
    const std::uint64_t window =
        (std::uint64_t{config_.limits.resync_window_ms} * config_.tick_rate + 999) / 1000;
    resync_window_ticks_ = static_cast<std::uint32_t>(std::max<std::uint64_t>(window, 1));
}

void World::run(std::stop_token stop) {
    const double fixed_dt = 1.0 / config_.tick_rate;
    const double max_accum = 0.25;
    const auto step_duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(fixed_dt));

    double accumulator = 0.0;
    auto last = std::chrono::steady_clock::now();
    while (!stop.stop_requested()) {
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> frame = now - last;
        last = now;

        const int steps = fixed_steps(accumulator, frame.count(), fixed_dt, max_accum);
        for (int i = 0; i < steps && !stop.stop_requested(); ++i) {
            tick(fixed_dt);  // always a fixed step -> deterministic simulation
        }
        std::this_thread::sleep_until(now + step_duration);
    }
}

void World::tick(double dt) {
    ++state_.tick;

    // Move the whole inbound queue into a tick-local one in one shot.
    swap(incoming_, local_);

    while (!local_.empty()) {
        auto ev = local_.try_pop();
        if (ev) {
            process_event(ev.value());
        }
    }

    consume_inputs(state_);
    integrate_movement(state_, config_, dt);
    index_alive_players();                           // positions are final for this tick
    activate_blocks(state_, config_);                // before attacks: same-tick blocks count
    resolve_attacks(state_, config_, alive_index_);  // melee hits + projectile launches
    update_projectiles(state_, config_, alive_index_, dt);
    update_captures(state_, config_);
    send_snapshots();
    close_released_sessions();
}

void World::index_alive_players() {
    alive_index_.clear();
    for (const auto& [session_id, p] : state_.players) {
        if (p.life == ::game::v1::LIFE_STATE_ALIVE) alive_index_.insert(session_id, p.x, p.y);
    }
}

void World::process_event(const ClientEvent& ev) {
    const std::uint64_t sid = ev.session_id;
    if (ev.kind == ClientEvent::Kind::Disconnected) {
        released_.erase(sid);
        on_disconnect(sid);
        return;
    }
    if (released_.contains(sid) || closing_.contains(sid)) {
        return;  // let go of: frames it sent before the close are still arriving
    }

    const auto& msg = ev.msg;
    const bool joined = state_.players.contains(sid);
    if (msg.payload_case() == ::game::v1::ClientMessage::kHello) {
        if (joined) {
            send_error(sid, ::game::v1::ERROR_CODE_UNEXPECTED_MESSAGE, msg.request_id(),
                       "a second Hello on one connection");
        } else {
            on_hello(sid, msg.hello(), msg.request_id());
        }
        return;
    }
    if (!joined) {
        send_error(sid, ::game::v1::ERROR_CODE_UNEXPECTED_MESSAGE, msg.request_id(),
                   "a message before Hello");
        return;
    }

    switch (msg.payload_case()) {
        case ::game::v1::ClientMessage::kSpawn:
            on_spawn(sid, msg.spawn(), msg.request_id());
            break;
        case ::game::v1::ClientMessage::kInput:
            on_input(sid, msg.input());
            break;
        case ::game::v1::ClientMessage::kPing:
            on_ping(sid, msg.ping());
            break;
        case ::game::v1::ClientMessage::kHello:  // handled above
        case ::game::v1::ClientMessage::PAYLOAD_NOT_SET:
        default:
            send_error(sid, ::game::v1::ERROR_CODE_UNSUPPORTED_MESSAGE, msg.request_id(),
                       "an empty or unknown payload");
            break;
    }
}

void World::on_hello(std::uint64_t session_id, const ::game::v1::Hello& hello,
                     std::uint32_t request_id) {
    auto name = check_hello(hello);
    if (!name) {
        send_error(session_id, name.error(), request_id,
                   name.error() == ::game::v1::ERROR_CODE_PROTOCOL_VERSION
                       ? "unsupported protocol_version"
                       : "the name must be 1-16 printable characters");
        return;
    }
    Player player;
    player.id = state_.next_player_id++;
    player.name = *std::move(name);
    state_.players[session_id] = player;

    // Greet the joiner: Welcome, then the map (as far as it sees: nothing yet —
    // snapshots reveal the rest), then the full roster.
    send(session_id, make_welcome(state_, config_, player));
    send(session_id, make_map_state(state_, player.sync.vision));
    send(session_id, make_full_roster(state_));

    // Tell everyone else that a new player joined.
    broadcast_except(session_id, make_roster_upsert(player));

    spdlog::info("World::on_hello session={} name='{}' -> player_id={}", session_id, player.name,
                 player.id);
}

void World::on_spawn(std::uint64_t session_id, const ::game::v1::SpawnRequest& spawn,
                     std::uint32_t request_id) {
    auto it = state_.players.find(session_id);
    if (it == state_.players.end()) {
        return;  // never sent Hello
    }
    Player& player = it->second;
    if (auto spawned = try_spawn(state_, config_, player, spawn.cell(), spawn.faction_id());
        !spawned) {
        send_error(session_id, spawned.error(), request_id, "spawn refused");
        return;
    }

    // Faction (colour) chosen -> tell everyone else.
    broadcast_except(session_id, make_roster_upsert(player));
    spdlog::info("World::on_spawn session={} player_id={} cell={} faction={}", session_id,
                 player.id, spawn.cell(), player.faction_id);
}

void World::on_input(std::uint64_t session_id, const ::game::v1::Input& input) {
    auto it = state_.players.find(session_id);
    if (it == state_.players.end()) {
        return;
    }
    enqueue_frames(it->second, input, config_.limits);  // applied one per tick by consume_inputs
}

void World::on_ping(std::uint64_t session_id, const ::game::v1::Ping& ping) {
    send(session_id, make_pong(state_, ping.client_time_ms()));
}

void World::on_disconnect(std::uint64_t session_id) {
    auto it = state_.players.find(session_id);
    if (it == state_.players.end()) {
        return;  // connected but never sent Hello, or already gone
    }

    const std::uint32_t player_id = it->second.id;
    state_.players.erase(it);

    // Tell the remaining players the player left (broadcast now excludes it).
    broadcast(make_roster_removed(player_id));

    spdlog::info("World::on_disconnect session={} player_id={}", session_id, player_id);
}

void World::send_snapshots() {
    if (snapshot_interval_ == 0 || state_.tick % snapshot_interval_ != 0) {
        return;
    }
    // Every connected player gets a Snapshot — even before spawning — filtered by
    // fog of war: what its faction sees, computed once per faction.
    std::unordered_map<std::uint32_t, Vision> sight;  // faction id -> its vision
    for (auto& [session_id, recipient] : state_.players) {
        if (closing_.contains(session_id)) continue;
        const std::uint32_t faction = recipient.faction_id;
        if (!sight.contains(faction)) {
            sight.emplace(faction, compute_vision(state_, config_, faction));
        }
        const Vision& vision = sight.at(faction);
        // A frame to it was lost: resend what it may have missed — the roster in
        // full, then a snapshot revealing its whole sight anew. A drop now marks
        // it again (or closes it).
        const bool resync = recipient.sync.resync;
        recipient.sync.resync = false;
        if (resync) send(session_id, make_full_roster(state_));
        send(session_id, build_snapshot(state_, recipient, vision, resync));
        recipient.sync.vision = vision;  // what this client now knows it sees
    }
    // This period's cell changes and events are delivered; start the next one.
    state_.territory.dirty.clear();
    state_.events.clear();
}

bool World::send(std::uint64_t session_id, const ::game::v1::ServerMessage& msg) {
    if (closing_.contains(session_id) || released_.contains(session_id)) {
        return false;  // given up on: nothing more goes out
    }
    std::vector<std::byte> bytes(msg.ByteSizeLong());
    if (!msg.SerializeToArray(bytes.data(), static_cast<int>(bytes.size()))) {
        spdlog::error("World::send failed to serialize for session={}", session_id);
        return false;
    }
    if (gateway_.send_to(session_id, std::move(bytes))) return true;
    on_dropped(session_id);
    return false;
}

void World::on_dropped(std::uint64_t session_id) {
    auto it = state_.players.find(session_id);
    if (it == state_.players.end()) return;  // not in the world (yet or anymore)
    if (note_drop(it->second.sync, state_.tick, resync_window_ticks_) == DropVerdict::Close) {
        spdlog::warn("World: session={} player_id={} keeps falling behind, closing", session_id,
                     it->second.id);
        closing_.try_emplace(session_id, ::game::v1::ERROR_CODE_UNSPECIFIED);  // nothing to tell
    }
}

void World::send_error(std::uint64_t session_id, ::game::v1::ErrorCode code,
                       std::uint32_t request_id, std::string_view detail) {
    send(session_id, make_error(code, request_id, detail));
    const bool fatal = is_fatal(code);
    spdlog::warn("World: session={} {} {} ({}), request_id={}", session_id,
                 fatal ? "closing with" : "refused:", ::game::v1::ErrorCode_Name(code), detail,
                 request_id);
    if (fatal) closing_.try_emplace(session_id, code);
}

void World::close_released_sessions() {
    // Leaving the world is broadcast, which may make others fall behind too.
    while (!closing_.empty()) {
        const auto [session_id, reason] = *closing_.begin();
        gateway_.disconnect(session_id, reason);  // after the frames already queued
        released_.insert(session_id);
        on_disconnect(session_id);  // leaves the world now; its Disconnected comes later
        closing_.erase(session_id);
    }
}

void World::broadcast(const ::game::v1::ServerMessage& msg) {
    for (const auto& [session_id, player] : state_.players) {
        send(session_id, msg);
    }
}

void World::broadcast_except(std::uint64_t session_id, const ::game::v1::ServerMessage& msg) {
    for (const auto& [sid, player] : state_.players) {
        if (sid != session_id) {
            send(sid, msg);
        }
    }
}
}  // namespace lit::game
