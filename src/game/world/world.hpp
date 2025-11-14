#pragma once

#include <vector>
#include <mutex>
#include <memory>

#include "player/i_player.hpp"

namespace ep::game
{
  class World {
  public:
    World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    ~World() = default;

    void GameLoop();
  private:
    void Tick();
    void AddPlayer();
    void RemovePlayer();
    void Broadcast();

    std::vector<std::shared_ptr<IPlayer>> players_;
  };
}
