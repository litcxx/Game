#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stop_token>
#include <unordered_map>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "net/client_envelope.hpp"
#include "net/i_client_gateway.hpp"
#include "player/i_player.hpp"
#include "utils/ts_queue.hpp"

namespace lit::game {
// Authoritative game loop. Consumes client messages (tagged with their session)
// from the inbound queue, dispatches by type, and sends results back through the
// gateway. No mechanics yet — the per-type handlers are stubs.
class World {
  public:
    World(TSQueue<ClientEnvelope>& incoming_msgs, IClientGateway& gateway, const GameConfig& config);
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    ~World() = default;

    // Runs the tick loop until the stop token is triggered (jthread supplies it).
    void run(std::stop_token stop);

  private:
    void tick(double dt);
    void send_snapshots();
    void process_input(const ClientEnvelope& env);

    // Per-type dispatch stubs (no mechanics yet).
    void on_hello(std::uint64_t session_id, const ::game::v1::Hello& hello);
    void on_spawn(std::uint64_t session_id, const ::game::v1::SpawnRequest& spawn);
    void on_input(std::uint64_t session_id, const ::game::v1::Input& input);
    void on_ping(std::uint64_t session_id, const ::game::v1::Ping& ping);

    // Serialize a server message and hand it to one session via the gateway.
    void send(std::uint64_t session_id, const ::game::v1::ServerMessage& msg);

    TSQueue<ClientEnvelope>& incoming_msgs_;
    TSQueue<ClientEnvelope> game_incoming_msgs_;  // tick-local double buffer
    IClientGateway& gateway_;
    const GameConfig config_;
    mutable std::mutex players_mutex_;
    std::unordered_map<std::size_t, std::shared_ptr<IPlayer>> players_;
};
}  // namespace lit::game
