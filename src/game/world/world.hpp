#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>

#include "player/i_player.hpp"
#include "utils/ts_queue.hpp"
#include "protocol/base_packet.hpp"
#include "protocol/network_subsystem.hpp"

namespace ep::game
{
  class World {
  public:
    World(std::shared_ptr<net::NetworkSubsystem> net_subsystem, uint8_t tick_rate);
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    ~World() = default;

    void GameLoop();
    void AddPlayer(std::shared_ptr<IPlayer> player);
    void RemovePlayer(std::size_t id);
    std::size_t PlayerNumbers() const;
  private:
    void Tick(double dt);
    void ProcessInput(net::GamePacket packet);

    std::shared_ptr<net::NetworkSubsystem> net_subsystem_;
    uint8_t tick_rate_;
    mutable std::mutex players_mutex_;
    std::unordered_map<std::size_t, std::shared_ptr<IPlayer>> players_;
    ep::utils::TSQueue<net::GamePacket> game_in_queue_;
  };
}
