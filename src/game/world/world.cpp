#include "world.hpp"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

#include <spdlog/spdlog.h>

#include "player/i_player.hpp"
#include "protocol/base_packet.hpp"
#include "protocol/events.hpp"
#include "protocol/opcodes.hpp"
#include "utils/ts_queue.hpp"
#include "player/player.hpp"

namespace ep::game
{
  World::World(std::shared_ptr<net::NetworkSubsystem> net_subsystem, std::shared_ptr<game::GameSubsystem> game_subsystem, std::uint8_t tick_rate) :
    net_subsystem_(net_subsystem),
    game_subsystem_(game_subsystem),
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
    // Double buffering event queue.
    ep::TSSwap(game_subsystem_->event_queue_, net_subsystem_->event_queue_);

    // Double buffering incoming queue.
    ep::TSSwap(game_subsystem_->in_queue_, net_subsystem_->in_queue_);

    // Handle events.
    while (!game_subsystem_->event_queue_.Empty()) {
      auto event = game_subsystem_->event_queue_.TryPop();
      switch (event->GetEvent()) {
      case ep::EventCode::AddNewPlayer: {
        spdlog::info("game event: AddNewPlayer id: {}", event->GetID());
        auto player  = std::make_shared<Player>(0, 0, event->GetID());
        AddPlayer(player);
        break;
      }
      case ep::EventCode::RemovePlayer: {
        spdlog::info("game event: remove player");
        RemovePlayer(event->GetID());
        break;
      }
      default:
        spdlog::info("game event: unknown event");
      }
    }

    // Pop packet from queue and process it.
    while (!game_subsystem_->in_queue_.Empty()) {
      spdlog::info("World::Tick: handle packet");
      auto packet = game_subsystem_->in_queue_.TryPop();
      ProcessInput(std::move(*packet));
    }

    // Push all packets to network queue for broadcast.
    while (!game_subsystem_->out_queue_.Empty()) {
      auto packet = game_subsystem_->out_queue_.TryPop();
      spdlog::info("game_subsystem_->out_queue_.TryPop");
      net_subsystem_->out_queue_.Push(std::move(packet));
    }
  }

  void World::ProcessInput(net::NetPacket packet)
  {
    net::NetPacket new_packet;
    // TODO check is this player exists
    auto player = players_[packet.GetID()];
    std::uint16_t opcode = packet.GetHeadOpcode();

    // TODO Collision
    // TODO Phisics 
    switch (to_opcode(opcode)) {
    case Opcodes::MoveForward: 
      spdlog::info("Opcode::MoveForward");
      player->Move(0, 1);
      OpcodeMovePlayer(new_packet, player);
      break;
    case Opcodes::MoveRight:
      spdlog::info("Opcodes::MoveRight");
      // player->Move(1, 0, 0);
      // OpcodeMovePlayer(new_packet, player);
      break;
    case Opcodes::MoveBackward:
      spdlog::info("Opcodes::MoveBackward");
      // player->Move(0, -1, 0);
      // OpcodeMovePlayer(new_packet, player);
      break;
    case Opcodes::MoveLeft:
      spdlog::info("Opcodes::MoveLeft");
      // player->Move(-1, 0, 0);
      // OpcodeMovePlayer(new_packet, player);
      break;
    default:
      spdlog::warn("unknown opcode: {}", opcode);
    }
    
    game_subsystem_->out_queue_.Push(std::move(new_packet));
  }

  void World::OpcodeMovePlayer(net::NetPacket& packet, std::shared_ptr<IPlayer> player)
  {
    packet.SetHeadOpcode(to_uint16(ep::Opcodes::MovePlayer));
    spdlog::info("id: {} x: {} y: {}", player->GetID(), player->GetX(), player->GetY());
    packet << player->GetID() << player->GetX() << player->GetY();
    spdlog::info("head size: {}", packet.GetHeadSize());
    spdlog::info("payload size: {}", packet.GetBodySize());
  }

  void World::AddPlayer(std::shared_ptr<IPlayer> player)
  {
    spdlog::info("World::AddPlayer");
    std::lock_guard lock(players_mutex_);
    // TODO check if this id already not used
    players_[player->GetID()] = player;
  }

  void World::RemovePlayer(std::size_t id)
  {
    spdlog::info("World::RemovePlayer");
    std::lock_guard lock(players_mutex_);
    // TODO check if this id is exists
    players_.erase(id);
  }
  std::size_t World::PlayerNumbers() const
  {
    std::lock_guard lock(players_mutex_);
    return players_.size(); 
  }
}
