#include "world.hpp"

#include <chrono>

#include <spdlog/spdlog.h>

#include "protocol/base_packet.hpp"

namespace ep::game
{
  World::World()
  {
    // TODO initialzie world
  }

  void World::GameLoop()
  {
    for (;;) {
      
    }
  }

  void World::Tick(float dt)
  {
    spdlog::info("World::Tick");
    game_queue_ = std::move(net_in_queue_);
    net::PacketData packet;
    while (game_queue_.TryPop(packet)) {
      // TODO handle packet
      spdlog::info("handle packet");
    }
  }
  
  void World::AddPlayer()
  {
    // TODO add player
  }

  void World::RemovePlayer()
  {
    // TODO remove player
  }

  void World::Broadcast()
  {
    // TODO broadcast
  }

}
