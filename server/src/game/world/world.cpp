#include "world.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <thread>
#include <vector>

namespace lit::game {
World::World(TSQueue<ClientEnvelope>& incoming_msgs, IClientGateway& gateway,
             const GameConfig& config)
    : incoming_msgs_{incoming_msgs}, gateway_{gateway}, config_{config} {
    // TODO: initialize map/territory state from config (later from persistence).
}

void World::run(std::stop_token stop) {
    const double tick_seconds = 1.0 / config_.tick_rate;
    const auto tick_duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(tick_seconds));

    auto last_tick = std::chrono::steady_clock::now();
    while (!stop.stop_requested()) {
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> delta = now - last_tick;
        last_tick = now;

        tick(delta.count());
        send_snapshots();

        std::this_thread::sleep_until(now + tick_duration);
    }
}

void World::tick(double dt) {
    (void)dt;  // no simulation yet

    // Double buffer: move the whole inbound queue into a tick-local one in one shot,
    // so we don't hold the shared queue's lock while processing messages.
    swap(incoming_msgs_, game_incoming_msgs_);

    while (!game_incoming_msgs_.empty()) {
        auto env = game_incoming_msgs_.try_pop();
        if (env) process_input(env.value());
    }
}

void World::process_input(const ClientEnvelope& env) {
    switch (env.msg.payload_case()) {
        case ::game::v1::ClientMessage::kHello:
            on_hello(env.session_id, env.msg.hello());
            break;
        case ::game::v1::ClientMessage::kSpawn:
            on_spawn(env.session_id, env.msg.spawn());
            break;
        case ::game::v1::ClientMessage::kInput:
            on_input(env.session_id, env.msg.input());
            break;
        case ::game::v1::ClientMessage::kPing:
            on_ping(env.session_id, env.msg.ping());
            break;
        case ::game::v1::ClientMessage::PAYLOAD_NOT_SET:
        default:
            spdlog::warn("World: empty/unknown payload from session={}", env.session_id);
            break;
    }
}

// --- dispatch stubs (no mechanics yet) ---

void World::on_hello(std::uint64_t session_id, const ::game::v1::Hello& hello) {
    spdlog::info("World::on_hello session={} name='{}' protocol_version={}", session_id,
                 hello.name(), hello.protocol_version());
    // TODO: assign player_id + session token, reply Welcome, then MapState + Roster.
}

void World::on_spawn(std::uint64_t session_id, const ::game::v1::SpawnRequest& spawn) {
    spdlog::info("World::on_spawn session={} cell={} faction_id={}", session_id, spawn.cell(),
                 spawn.faction_id());
    // TODO: validate cell/faction, place the player, mark ALIVE.
}

void World::on_input(std::uint64_t session_id, const ::game::v1::Input& input) {
    spdlog::info("World::on_input session={} frames={}", session_id, input.frames_size());
    // TODO: apply movement/attack intents for this tick.
}

void World::on_ping(std::uint64_t session_id, const ::game::v1::Ping& ping) {
    // Transport keepalive: echo a Pong (server_tick is a stub until we count ticks).
    ::game::v1::ServerMessage reply;
    auto* pong = reply.mutable_pong();
    pong->set_client_time_ms(ping.client_time_ms());
    pong->set_server_tick(0);
    send(session_id, reply);
}

void World::send_snapshots() {
    // TODO: build a per-session Snapshot and broadcast at snapshot_rate. Stub for now.
}

void World::send(std::uint64_t session_id, const ::game::v1::ServerMessage& msg) {
    std::vector<std::byte> bytes(msg.ByteSizeLong());
    if (!msg.SerializeToArray(bytes.data(), static_cast<int>(bytes.size()))) {
        spdlog::error("World::send failed to serialize for session={}", session_id);
        return;
    }
    gateway_.send_to(session_id, std::move(bytes));
}
}  // namespace lit::game
