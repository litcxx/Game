#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

#include "config/config.hpp"
#include "protocol/server_packet.hpp"
#include "subsystems/network_subsystem.hpp"
#include "subsystems/game_subsystem.hpp"
#include "player/i_player.hpp"
#include "tile/tile.hpp"
#include "physics/collision.hpp"

namespace ep::game
{
  class World {
  public:
    explicit World(std::shared_ptr<NetworkSubsystem> net_subsystem, 
                   std::shared_ptr<GameSubsystem> game_subsystem, 
                   const GameConfig& config);
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    ~World() = default;

    void GameLoop();
    void AddPlayer(std::shared_ptr<IPlayer> player);
    void RemovePlayer(std::size_t id);
    std::size_t PlayerNumbers() const;
  private:
    void Tick(double dt);
    void ProcessInput(std::unique_ptr<ServerPacket> packet, double dt);
    void Update(IPlayer& player, double dt);

    void MovePlayer(IPlayer& player);

    std::shared_ptr<NetworkSubsystem> net_subsystem_;
    std::shared_ptr<GameSubsystem> game_subsystem_;
    const GameConfig& config_;
    mutable std::mutex players_mutex_;
    std::unordered_map<std::size_t, std::shared_ptr<IPlayer>> players_;
    std::vector<Tile> map_;
    Collision collision_;
  };
}
