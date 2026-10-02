#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "net/client_event.hpp"
#include "net/i_client_gateway.hpp"
#include "persist/save_sink.hpp"
#include "persist/world_save.hpp"
#include "save.pb.h"
#include "spatial/spatial_index.hpp"
#include "state/world_state.hpp"
#include "systems/spawn_system.hpp"
#include "utils/ts_queue.hpp"
#include "world/connection.hpp"
#include "world/fixed_step.hpp"
#include "world/metrics.hpp"

namespace lit::game {
// Authoritative game loop. Drains net→game events each tick, updates state, and
// replies through the gateway. Runs on a single (game) thread — no locking.
// Every `metrics_interval` it logs what it did as one `metrics {json}` line.
// With a save sink it hands it a snapshot of the world every save interval and
// once more when the loop stops (GAME-019).
class World {
  public:
    // Characters spawn at their faction's capital (capital_spawn_point).
    World(TSQueue<ClientEvent>& incoming, IClientGateway& gateway, const GameConfig& config,
          std::chrono::seconds metrics_interval = std::chrono::seconds{60});
    // Characters spawn where `spawn_point` says (tests place bodies with it).
    World(TSQueue<ClientEvent>& incoming, IClientGateway& gateway, const GameConfig& config,
          SpawnPoint spawn_point, std::chrono::seconds metrics_interval = std::chrono::seconds{60});
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    ~World() = default;

    // Ticks at the fixed rate until `stop`; then hands the save sink the world as
    // it stops (SIGTERM).
    void run(std::stop_token stop);

    // Before the first tick: the saved world comes back (restore_world). A save
    // this server can't take is refused with why, the world left new.
    std::expected<SeasonLoad, std::string> restore(const ::lit::save::WorldSave& save);
    // From now on a snapshot of the world goes to `sink` every `interval`
    // (rounded to ticks, at least one); `sink` outlives the World.
    void save_to(ISaveSink& sink, std::chrono::seconds interval);
    // A snapshot to the save sink now (none attached: nothing). Exposed for tests.
    void save_now();

    // Advances one tick: drains and processes queued events. Exposed for tests.
    void tick(double dt);

  private:
    void process_event(const ClientEvent& ev);
    bool is_snapshot_tick() const { return state_.tick % snapshot_interval_ == 0; }
    void send_snapshots();
    // Once a second: the faction scores to everyone (GAME-018).
    void send_faction_scores();
    // End of tick: record how long it took; log the period's metrics when it ends.
    void finish_tick_metrics(std::chrono::steady_clock::time_point started);

    // Per-message handlers. `request_id` is echoed in any error they send.
    // Hello joins a new character or, with a known session token, resumes one.
    void on_hello(std::uint64_t session_id, const ::game::v1::Hello& hello,
                  std::uint32_t request_id);
    void on_spawn(std::uint64_t session_id, const ::game::v1::SpawnRequest& spawn,
                  std::uint32_t request_id);
    void on_input(std::uint64_t session_id, const ::game::v1::Input& input);
    void on_ping(std::uint64_t session_id, const ::game::v1::Ping& ping);
    // The session stops driving its character, which stays in the world, away,
    // for the reconnect grace.
    void on_disconnect(std::uint64_t session_id);

    // `session_id` drives a known character again: taken over from the session
    // driving it (SESSION_REPLACED), or back from away or out of the world.
    void resume(std::uint64_t session_id, std::uint32_t character_id,
                std::string_view session_token);
    // Welcome, MapState and the full roster to a session that now drives `character`.
    void greet(std::uint64_t session_id, const Character& character, std::string_view session_token,
               bool resumed);
    // Every tick: characters away past their reconnect grace leave the world.
    void end_reconnect_graces();

    // Rebuild the spatial index of alive units (after movement, before combat).
    void index_alive_units();

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
    const SpawnPoint spawn_point_;  // where characters come into the world

    std::uint32_t snapshot_interval_{1};      // ticks between snapshots (tick_rate / snapshot_rate)
    ISaveSink* save_sink_{nullptr};           // where world snapshots go; none: not saved
    std::uint32_t save_interval_ticks_{1};    // ticks between world snapshots
    std::uint32_t scores_interval_ticks_{1};  // ticks between faction scores: a second
    std::uint32_t resync_window_ticks_{1};    // config.limits.resync_window_ms in ticks
    std::uint32_t grace_ticks_{0};            // config.reconnect_grace_ms in ticks
    TickLimits limits_;                       // config.limits in ticks
    // Every open connection, from Connected (or its first message) to Disconnected.
    std::unordered_map<std::uint64_t, Connection> connections_;
    // Sessions to close at the end of the tick, with the reason (UNSPECIFIED = a
    // plain close); once closed they are released: whatever they still send is
    // ignored until their Disconnected event.
    std::unordered_map<std::uint64_t, ::game::v1::ErrorCode> closing_;
    std::unordered_set<std::uint64_t> released_;
    WorldState state_;          // all simulation state (plain data)
    SpatialIndex alive_index_;  // alive units by position, rebuilt each tick

    Metrics metrics_;
    std::uint32_t metrics_interval_s_;
    std::uint32_t metrics_interval_ticks_;
};
}  // namespace lit::game
