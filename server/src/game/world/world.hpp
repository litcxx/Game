#pragma once

#include <cstdint>
#include <stop_token>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "net/client_event.hpp"
#include "net/i_client_gateway.hpp"
#include "state/world_state.hpp"
#include "utils/ts_queue.hpp"
#include "world/fixed_step.hpp"

namespace lit::game {
// Authoritative game loop. Drains net→game events each tick, updates state, and
// replies through the gateway. Runs on a single (game) thread — no locking.
class World {
  public:
    World(TSQueue<ClientEvent>& incoming, IClientGateway& gateway, const GameConfig& config);
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    ~World() = default;

    void run(std::stop_token stop);

    // Advances one tick: drains and processes queued events. Exposed for tests.
    void tick(double dt);

  private:
    void process_event(const ClientEvent& ev);
    void send_snapshots();

    // Per-message handlers.
    void on_hello(std::uint64_t session_id, const ::game::v1::Hello& hello);
    void on_spawn(std::uint64_t session_id, const ::game::v1::SpawnRequest& spawn);
    void on_input(std::uint64_t session_id, const ::game::v1::Input& input);
    void on_ping(std::uint64_t session_id, const ::game::v1::Ping& ping);
    void on_disconnect(std::uint64_t session_id);

    // Per-tick combat: alive attackers hit their target when in range and off cooldown.
    void update_combat();

    // Message builders.
    ::game::v1::ServerMessage make_welcome(const Player& player) const;
    ::game::v1::ServerMessage make_map_state() const;
    ::game::v1::ServerMessage make_full_roster() const;
    ::game::v1::ServerMessage make_roster_upsert(const Player& player) const;

    // Outbound helpers (serialize once, deliver via the gateway).
    void send(std::uint64_t session_id, const ::game::v1::ServerMessage& msg);
    void broadcast(const ::game::v1::ServerMessage& msg);
    void broadcast_except(std::uint64_t session_id, const ::game::v1::ServerMessage& msg);

    TSQueue<ClientEvent>& incoming_;
    TSQueue<ClientEvent> local_;  // tick-local double buffer
    IClientGateway& gateway_;
    const GameConfig config_;

    std::uint32_t snapshot_interval_{1};  // ticks between snapshots (tick_rate / snapshot_rate)
    WorldState state_;                    // all simulation state (plain data)
};
}  // namespace lit::game
