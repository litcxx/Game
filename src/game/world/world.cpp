#include "world.hpp"
#include "utils/ts_queue.hpp"

#include <chrono>
#include <system_error>
#include <thread>

#include <spdlog/spdlog.h>

namespace ep::game
{
  World::World(std::shared_ptr<net::NetworkSubsystem> net_subsystem) :
    net_subsystem_(net_subsystem)
  {
    // TODO initialzie world
  }

  void World::GameLoop()
  {
    constexpr double tick_rate = 60.0;
    spdlog::info("tick rate: {} Hz", tick_rate);
    
    constexpr double tick_seconds = 1.0 / tick_rate;
    auto tick_duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(tick_seconds));

    auto last = std::chrono::steady_clock::now();

    for (;;) {
      auto current = std::chrono::steady_clock::now();
      std::chrono::duration<double> delta = current - last;
      last = current;

      // spdlog::info("delta = {}, actual Hz = {}", delta.count(), 1.0 / delta.count());

      Tick(delta.count());

      std::this_thread::sleep_until(current + tick_duration);
    }
  }

  void World::Tick(float dt)
  {
    // spdlog::info("World::Tick");
    utils::TSSwap(game_queue_, net_subsystem_->net_in_queue_);
    net::PacketData packet;
    while (game_queue_.TryPop(packet)) {
      // TODO handle packet
      spdlog::info("World::Tick: handle packet");
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
