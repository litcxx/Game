#pragma once

#include <cstdint>
#include <stop_token>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "net/client_event.hpp"
#include "net/i_client_gateway.hpp"
#include "spatial/spatial_index.hpp"
#include "state/world_state.hpp"
#include "utils/ts_queue.hpp"
#include "world/connection.hpp"
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

    // Per-message handlers. `request_id` is echoed in any error they send.
    void on_hello(std::uint64_t session_id, const ::game::v1::Hello& hello,
                  std::uint32_t request_id);
    void on_spawn(std::uint64_t session_id, const ::game::v1::SpawnRequest& spawn,
                  std::uint32_t request_id);
    void on_input(std::uint64_t session_id, const ::game::v1::Input& input);
    void on_ping(std::uint64_t session_id, const ::game::v1::Ping& ping);
    void on_disconnect(std::uint64_t session_id);

    // Rebuild the spatial index of alive players (after movement, before combat).
    void index_alive_players();

    // Outbound helpers (serialize once, deliver via the gateway). A refused frame
    // marks its recipient for a resync — or for closing (see note_drop).
    bool send(std::uint64_t session_id, const ::game::v1::ServerMessage& msg);
    void broadcast(const ::game::v1::ServerMessage& msg);
    void broadcast_except(std::uint64_t session_id, const ::game::v1::ServerMessage& msg);
    void on_dropped(std::uint64_t session_id);
    // Tell a session why its request failed (ServerError). A fatal error also
    // releases it: closed with 4000 + code at the end of the tick.
    void send_error(std::uint64_t session_id, ::game::v1::ErrorCode code, std::uint32_t request_id,
                    std::string_view detail);
    // End of tick: close the sessions being released (a fatal error, or falling
    // behind again); they leave the world.
    void close_released_sessions();
    // Every tick: a connection with no Hello in time or silent for too long is
    // told so (HANDSHAKE_TIMEOUT / IDLE_TIMEOUT) and released.
    void time_out_connections();

    TSQueue<ClientEvent>& incoming_;
    TSQueue<ClientEvent> local_;  // tick-local double buffer
    IClientGateway& gateway_;
    const GameConfig config_;

    std::uint32_t snapshot_interval_{1};    // ticks between snapshots (tick_rate / snapshot_rate)
    std::uint32_t resync_window_ticks_{1};  // config.limits.resync_window_ms in ticks
    TickLimits limits_;                     // config.limits in ticks
    // Every open connection, from Connected (or its first message) to Disconnected.
    std::unordered_map<std::uint64_t, Connection> connections_;
    // Sessions to close at the end of the tick, with the reason (UNSPECIFIED = a
    // plain close); once closed they are released: whatever they still send is
    // ignored until their Disconnected event.
    std::unordered_map<std::uint64_t, ::game::v1::ErrorCode> closing_;
    std::unordered_set<std::uint64_t> released_;
    WorldState state_;          // all simulation state (plain data)
    SpatialIndex alive_index_;  // alive players by position, rebuilt each tick
};
}  // namespace lit::game
