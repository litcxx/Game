#pragma once

#include <vector>
#include <memory>

#include "player/i_player.hpp"
#include "utils/ts_queue.hpp"
#include "protocol/base_packet.hpp"
#include "protocol/network_subsystem.hpp"

namespace ep::game
{
  class World {
  public:
    World(std::shared_ptr<net::NetworkSubsystem> net_subsystem);
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    ~World() = default;

    void GameLoop();
  private:
    void Tick(float dt);
    void AddPlayer();
    void RemovePlayer();
    void Broadcast();

    std::shared_ptr<net::NetworkSubsystem> net_subsystem_;
    std::vector<std::shared_ptr<IPlayer>> players_;
    ep::utils::TSQueue<net::PacketData> game_queue_;
  };
}
