#include "world.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
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
#include "systems/presence_system.hpp"
#include "systems/projectile_system.hpp"
#include "systems/spawn_system.hpp"
#include "systems/vision_system.hpp"
#include "utils/session_token.hpp"

namespace lit::game {
namespace {
// Spatial-index bucket edge: 2 cells, so a melee query spans at most 3x3 buckets.
constexpr double kIndexBucketUnits = 2.0 * kUnitsPerCell;
}  // namespace

World::World(TSQueue<ClientEvent>& incoming, IClientGateway& gateway, const GameConfig& config,
             std::chrono::seconds metrics_interval)
    : incoming_{incoming},
      gateway_{gateway},
      config_{config},
      limits_{TickLimits::from(config.limits, config.tick_rate)},
      alive_index_{static_cast<double>(config.map_width * kUnitsPerCell),
                   static_cast<double>(config.map_height * kUnitsPerCell), kIndexBucketUnits},
      // At least a second (and a tick): the config refuses 0 anyway.
      metrics_interval_s_{
          static_cast<std::uint32_t>(std::max<std::int64_t>(metrics_interval.count(), 1))},
      metrics_interval_ticks_{std::max<std::uint32_t>(metrics_interval_s_ * config.tick_rate, 1)} {
    state_.territory.reset(config_.map_width, config_.map_height);

    const std::uint32_t rate = config_.snapshot_rate == 0 ? 1 : config_.snapshot_rate;
    snapshot_interval_ = config_.tick_rate / rate;
    if (snapshot_interval_ == 0) snapshot_interval_ = 1;

    // Rounded up: a window shorter than a tick still spans one.
    const std::uint64_t window =
        (std::uint64_t{config_.limits.resync_window_ms} * config_.tick_rate + 999) / 1000;
    resync_window_ticks_ = static_cast<std::uint32_t>(std::max<std::uint64_t>(window, 1));
    grace_ticks_ = static_cast<std::uint32_t>(
        (std::uint64_t{config_.reconnect_grace_ms} * config_.tick_rate + 999) / 1000);
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
    const auto started = std::chrono::steady_clock::now();
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
    index_alive_units();                             // positions are final for this tick
    activate_blocks(state_, config_);                // before attacks: same-tick blocks count
    resolve_attacks(state_, config_, alive_index_);  // melee hits + projectile launches
    update_projectiles(state_, config_, alive_index_, dt);
    update_captures(state_, config_);
    time_out_connections();
    end_reconnect_graces();
    send_snapshots();
    close_released_sessions();
    finish_tick_metrics(started);
}

void World::finish_tick_metrics(std::chrono::steady_clock::time_point started) {
    const auto took = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started);
    metrics_.record_tick(static_cast<std::uint32_t>(took.count()), is_snapshot_tick());
    if (state_.tick % metrics_interval_ticks_ != 0) return;
    const auto ccu = static_cast<std::uint32_t>(state_.sessions.size());
    spdlog::info("metrics {}", to_json(metrics_.report(metrics_interval_s_, ccu)));
}

void World::index_alive_units() {
    alive_index_.clear();
    for (const auto& [id, u] : state_.units) {
        if (u.life == ::game::v1::LIFE_STATE_ALIVE) alive_index_.insert(id, u.x, u.y);
    }
}

void World::process_event(const ClientEvent& ev) {
    const std::uint64_t sid = ev.session_id;
    if (ev.kind == ClientEvent::Kind::Connected) {
        connections_.try_emplace(sid, open_connection(state_.tick, limits_));
        return;
    }
    if (ev.kind == ClientEvent::Kind::Disconnected) {
        released_.erase(sid);
        connections_.erase(sid);
        on_disconnect(sid);
        return;
    }
    if (released_.contains(sid) || closing_.contains(sid)) {
        return;  // let go of: frames it sent before the close are still arriving
    }

    const auto& msg = ev.msg;
    // Opened on its first message if its Connected was not seen (e.g. in tests).
    Connection& connection =
        connections_.try_emplace(sid, open_connection(state_.tick, limits_)).first->second;
    if (!take_message(connection, state_.tick, limits_)) {
        send_error(sid, ::game::v1::ERROR_CODE_RATE_LIMITED, msg.request_id(),
                   "over the message budget");
        return;
    }
    const bool joined = state_.sessions.contains(sid);
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
    // A known token brings its character back, whatever name comes with it; an
    // unknown one (e.g. from before a restart) is no different from none.
    if (!hello.session_token().empty()) {
        if (const Character* known = find_by_token(state_, hash_token(hello.session_token()))) {
            resume(session_id, known->id, hello.session_token());
            return;
        }
    }
    if (is_name_taken(state_, *name)) {
        send_error(session_id, ::game::v1::ERROR_CODE_INVALID_NAME, request_id,
                   "the name is taken");
        return;
    }
    const std::string token = new_session_token();
    const std::uint32_t character_id =
        create_character(state_, *std::move(name), hash_token(token)).id;
    attach_session(state_, session_id, character_id);
    const Character& character = state_.characters.at(character_id);
    greet(session_id, character, token, /*resumed=*/false);

    // Tell everyone else that a new player joined.
    broadcast_except(session_id, make_roster_upsert(state_, character));
    metrics_.record_join();

    spdlog::info("World::on_hello session={} name='{}' -> player_id={}", session_id, character.name,
                 character.id);
}

void World::resume(std::uint64_t session_id, std::uint32_t character_id,
                   std::string_view session_token) {
    const bool was_in_world = state_.characters.at(character_id).in_world;
    if (const auto replaced = attach_session(state_, session_id, character_id)) {
        // Taken over from another tab: that one is told why it is closed (unless
        // it is being closed anyway).
        if (!closing_.contains(*replaced)) {
            send_error(*replaced, ::game::v1::ERROR_CODE_SESSION_REPLACED, 0,
                       "the character was taken over by another connection");
        }
    }
    const Character& character = state_.characters.at(character_id);
    // Still in the world: the same state, which no one else notices. Back after
    // the grace: without a body, and the others see it join again.
    greet(session_id, character, session_token, /*resumed=*/was_in_world);
    if (was_in_world) {
        metrics_.record_resume();
    } else {
        broadcast_except(session_id, make_roster_upsert(state_, character));
        metrics_.record_join();
    }

    spdlog::info("World::on_hello session={} {} player_id={} name='{}'", session_id,
                 was_in_world ? "resumed" : "returned as", character.id, character.name);
}

void World::greet(std::uint64_t session_id, const Character& character,
                  std::string_view session_token, bool resumed) {
    // Welcome, then the map (as far as it sees: nothing yet — snapshots reveal
    // the rest), then the full roster.
    send(session_id, make_welcome(state_, config_, character, session_token, resumed));
    send(session_id, make_map_state(state_, state_.sessions.at(session_id).sync.vision));
    send(session_id, make_full_roster(state_));
}

void World::on_spawn(std::uint64_t session_id, const ::game::v1::SpawnRequest& spawn,
                     std::uint32_t request_id) {
    auto it = state_.sessions.find(session_id);
    if (it == state_.sessions.end()) {
        return;  // never sent Hello
    }
    ClientSession& session = it->second;
    const std::uint32_t character_id = session.character_id;
    if (auto spawned = try_spawn(state_, config_, character_id, spawn.cell(), spawn.faction_id());
        !spawned) {
        send_error(session_id, spawned.error(), request_id, "spawn refused");
        return;
    }
    // Drop stale pre-spawn commands (last_enqueued_seq stays monotonic).
    session.input.commands.clear();

    // Faction (colour) chosen -> tell everyone else.
    broadcast_except(session_id, make_roster_upsert(state_, state_.characters.at(character_id)));
    spdlog::info("World::on_spawn session={} player_id={} cell={} faction={}", session_id,
                 character_id, spawn.cell(), spawn.faction_id());
}

void World::on_input(std::uint64_t session_id, const ::game::v1::Input& input) {
    auto it = state_.sessions.find(session_id);
    if (it == state_.sessions.end()) {
        return;
    }
    enqueue_frames(it->second.input, input, config_.limits);  // one per tick: consume_inputs
}

void World::on_ping(std::uint64_t session_id, const ::game::v1::Ping& ping) {
    send(session_id, make_pong(state_, ping.client_time_ms()));
}

void World::on_disconnect(std::uint64_t session_id) {
    const auto character_id = detach_session(state_, session_id, state_.tick + grace_ticks_);
    if (!character_id) {
        return;  // connected but never sent Hello, taken over, or already gone
    }
    // The character stays in the world — visible and vulnerable — until its
    // reconnect grace ends; closing the tab does not take it out of a fight.
    spdlog::info("World::on_disconnect session={} player_id={}: away for {} ticks", session_id,
                 *character_id, grace_ticks_);
}

void World::end_reconnect_graces() {
    for (std::uint32_t character_id : leave_after_grace(state_, state_.tick)) {
        // Tell the remaining players the player left.
        broadcast(make_roster_removed(character_id));
        metrics_.record_leave();
        spdlog::info("World: player_id={} left the world: its reconnect grace is over",
                     character_id);
    }
}

void World::send_snapshots() {
    if (!is_snapshot_tick()) {
        return;
    }
    // Every session gets a Snapshot — even before spawning — filtered by fog of
    // war: what its character's faction sees, computed once per faction.
    std::unordered_map<std::uint32_t, Vision> sight;  // faction id -> its vision
    for (auto& [session_id, recipient] : state_.sessions) {
        if (closing_.contains(session_id)) continue;
        const std::uint32_t faction = faction_of(state_, recipient.character_id);
        if (!sight.contains(faction)) {
            sight.emplace(faction, compute_vision(state_, config_, faction));
        }
        const Vision& vision = sight.at(faction);
        // A frame to it was lost: resend what it may have missed — the roster in
        // full, then a snapshot revealing its whole sight anew. A drop now marks
        // it again (or closes it).
        const bool resync = recipient.sync.resync;
        recipient.sync.resync = false;
        if (resync) {
            metrics_.record_resync();
            send(session_id, make_full_roster(state_));
        }
        const auto snapshot = build_snapshot(state_, recipient, vision, resync);
        metrics_.record_snapshot(snapshot.ByteSizeLong());
        send(session_id, snapshot);
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
    metrics_.record_drop();
    auto it = state_.sessions.find(session_id);
    if (it == state_.sessions.end()) return;  // not in the world (yet or anymore)
    if (note_drop(it->second.sync, state_.tick, resync_window_ticks_) == DropVerdict::Close) {
        spdlog::warn("World: session={} player_id={} keeps falling behind, closing", session_id,
                     it->second.character_id);
        metrics_.record_closed_behind();
        closing_.try_emplace(session_id, ::game::v1::ERROR_CODE_UNSPECIFIED);  // nothing to tell
    }
}

void World::send_error(std::uint64_t session_id, ::game::v1::ErrorCode code,
                       std::uint32_t request_id, std::string_view detail) {
    send(session_id, make_error(code, request_id, detail));
    metrics_.record_error(code);
    const bool fatal = is_fatal(code);
    spdlog::warn("World: session={} {} {} ({}), request_id={}", session_id,
                 fatal ? "closing with" : "refused:", ::game::v1::ErrorCode_Name(code), detail,
                 request_id);
    if (fatal) closing_.try_emplace(session_id, code);
}

void World::time_out_connections() {
    for (const auto& [session_id, connection] : connections_) {
        if (closing_.contains(session_id) || released_.contains(session_id)) continue;
        const bool joined = state_.sessions.contains(session_id);
        if (const auto code = overdue(connection, joined, state_.tick, limits_)) {
            send_error(session_id, *code, 0,
                       *code == ::game::v1::ERROR_CODE_HANDSHAKE_TIMEOUT
                           ? "no Hello in time"
                           : "nothing received for too long");
        }
    }
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
    for (const auto& [session_id, session] : state_.sessions) {
        send(session_id, msg);
    }
}

void World::broadcast_except(std::uint64_t session_id, const ::game::v1::ServerMessage& msg) {
    for (const auto& [sid, session] : state_.sessions) {
        if (sid != session_id) {
            send(sid, msg);
        }
    }
}
}  // namespace lit::game
