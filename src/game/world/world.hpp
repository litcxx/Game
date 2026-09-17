#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "config/config.hpp"
#include "physics/collision.hpp"
#include "player/i_player.hpp"
#include "protocol/server_packet.hpp"
#include "subsystems/game_subsystem.hpp"
#include "subsystems/network_subsystem.hpp"
#include "tile/tile.hpp"

namespace lit::game {
class World {
  public:
    explicit World(std::shared_ptr<NetworkSubsystem> net_subsystem,
                   std::shared_ptr<GameSubsystem> game_subsystem, const GameConfig& config);
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    ~World() = default;

    void game_loop();
    void add_player(std::shared_ptr<IPlayer> player);
    void remove_player(std::size_t id);
    std::size_t player_numbers() const;

  private:
    void tick(double dt);
    void process_input(std::unique_ptr<ServerPacket> packet, double dt);
    void update(IPlayer& player, double dt);

    void move_player(IPlayer& player);

    std::shared_ptr<NetworkSubsystem> net_subsystem_;
    std::shared_ptr<GameSubsystem> game_subsystem_;
    const GameConfig& config_;
    mutable std::mutex players_mutex_;
    std::unordered_map<std::size_t, std::shared_ptr<IPlayer>> players_;
    std::vector<Tile> map_;
    Collision collision_;
};
}  // namespace lit::game
