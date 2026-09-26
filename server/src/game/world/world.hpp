#pragma once

#include <cstdint>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <vector>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "net/client_event.hpp"
#include "net/i_client_gateway.hpp"
#include "utils/ts_queue.hpp"

namespace lit::game {
// Per-connection game state. `session_id` is the transport key; `id` is the
// public player_id (>= 1) used on the wire.
struct Player {
    std::uint32_t id{0};
    std::string name;
    std::uint32_t faction_id{0};  // 0 until spawned
    ::game::v1::LifeState life{::game::v1::LIFE_STATE_NOT_SPAWNED};
    // M2+: position (x, y), hp, last_input_seq, cooldown/respawn ticks...
};

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

    std::uint32_t tick_{0};
    std::uint32_t next_player_id_{1};
    std::unordered_map<std::uint64_t, Player> players_;  // key: session_id
    std::vector<std::uint8_t> owners_;                   // map_w*map_h, 0 = neutral
};
}  // namespace lit::game
