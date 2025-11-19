#include "world.hpp"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

#include <spdlog/spdlog.h>

#include "player/i_player.hpp"
#include "protocol/opcodes.hpp"

namespace ep::game
{
  World::World(std::shared_ptr<net::NetworkSubsystem> net_subsystem, uint8_t tick_rate) :
    net_subsystem_(net_subsystem),
    tick_rate_(tick_rate)
  {
    // TODO initialzie world
  }

  void World::GameLoop()
  {
    const double tick_seconds = 1.0 / tick_rate_;
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
      players_[packet.GetID()]->Move(0, 1, 0);
      // TODO push packet to out
      break;
    case Opcodes::MoveRight:
      players_[packet.GetID()]->Move(1, 0, 0);
      // TODO push packet to out
      break;
    case Opcodes::MoveBackward:
      players_[packet.GetID()]->Move(0, -1, 0);
      // TODO push packet to out
      break;
    case Opcodes::MoveLeft:
      players_[packet.GetID()]->Move(-1, 0, 0);
      // TODO push packet to out
      break;
    default:
      // TODO push packet to out
      ;
    }
  }

  void World::AddPlayer(std::shared_ptr<IPlayer> player)
  {
    std::lock_guard lock(players_mutex_);
    players_[player->GetID()] = player;
  }

  void World::RemovePlayer(std::size_t id)
  {
    std::lock_guard lock(players_mutex_);
    players_.erase(id);
  }
  std::size_t World::PlayerNumbers() const
  {
    std::lock_guard lock(players_mutex_);
    return players_.size(); 
  }
}
