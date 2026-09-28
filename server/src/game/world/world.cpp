#include "world.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <thread>
#include <unordered_map>
#include <vector>

#include "state/vision.hpp"
#include "sync/messages.hpp"
#include "sync/snapshot_builder.hpp"
#include "systems/capture_system.hpp"
#include "systems/combat_system.hpp"
#include "systems/input_system.hpp"
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
}

void World::index_alive_players() {
    alive_index_.clear();
    for (const auto& [session_id, p] : state_.players) {
        if (p.life == ::game::v1::LIFE_STATE_ALIVE) alive_index_.insert(session_id, p.x, p.y);
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
    player.id = state_.next_player_id++;
    player.name = hello.name();
    state_.players[session_id] = player;

    // Greet the joiner: Welcome, then the map (as far as it sees: nothing yet —
    // snapshots reveal the rest), then the full roster.
    send(session_id, make_welcome(state_, config_, player));
    send(session_id, make_map_state(state_, player.vision));
    send(session_id, make_full_roster(state_));

    // Tell everyone else that a new player joined.
    broadcast_except(session_id, make_roster_upsert(player));

    spdlog::info("World::on_hello session={} name='{}' -> player_id={}", session_id, hello.name(),
                 player.id);
}

void World::on_spawn(std::uint64_t session_id, const ::game::v1::SpawnRequest& spawn) {
    auto it = state_.players.find(session_id);
    if (it == state_.players.end()) {
        return;  // never sent Hello
    }
    Player& player = it->second;
    if (!try_spawn(state_, config_, player, spawn.cell(), spawn.faction_id())) {
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
    enqueue_frames(it->second, input);  // applied one per tick by consume_inputs
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
        const std::uint32_t faction = recipient.faction_id;
        if (!sight.contains(faction)) {
            sight.emplace(faction, compute_vision(state_, config_, faction));
        }
        const Vision& vision = sight.at(faction);
        send(session_id, build_snapshot(state_, recipient, vision));
        recipient.vision = vision;  // what this client now knows it sees
    }
    // This period's cell changes and events are delivered; start the next one.
    state_.territory.dirty.clear();
    state_.events.clear();
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
