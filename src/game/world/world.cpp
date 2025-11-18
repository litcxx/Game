#include "world.hpp"

#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>

#include "protocol/opcodes.cpp"

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

  void World::Tick(double dt)
  {
    // spdlog::info("World::Tick");
    utils::TSSwap(game_in_queue_, net_subsystem_->net_in_queue_);
    while (!game_in_queue_.Empty()) {
      spdlog::info("World::Tick: handle packet");
      auto packet = game_in_queue_.TryPop();
      ProcessInput(std::move(*packet));
    }
  }

  void World::ProcessInput(net::GamePacket packet)
  {
    using ep::protocol::Opcodes;

    switch (static_cast<Opcodes>(packet.GetOpcode())) {
    case Opcodes::MoveForward: 
      break;
    case Opcodes::MoveRight:
      break;
    case Opcodes::MoveBackward:
      break;
    case Opcodes::MoveLeft:
      break;
    default:
      ;
    }
  }

  void World::AddPlayer()
  {
    std::lock_guard lock(players_mutex_);
    // players_.push(std::make_shared<Player>(0, 0, 0, 1, ?));
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
