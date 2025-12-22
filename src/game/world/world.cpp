#include "world.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <thread>

#include <spdlog/spdlog.h>
#include "protocol/server_packet.hpp"
#include "spdlog/common.h"

#include "protocol/net_packet.hpp"
#include "protocol/opcodes.hpp"
#include "tile/tile.hpp"
#include "utils/ts_queue.hpp"
#include "player/player.hpp"

namespace ep::game
{
  World::World(std::shared_ptr<NetworkSubsystem> net_subsystem, 
               std::shared_ptr<GameSubsystem> game_subsystem, 
               const GameConfig& config) :
    net_subsystem_(net_subsystem),
    game_subsystem_(game_subsystem),
    config_(config)
  {
    // Initialzie map
    map_.resize(config_.grid_x_ * config_.grid_y_);
    for (std::size_t y = 0; y < config_.grid_y_; y++) {
      for (std::size_t x = 0; x < config_.grid_x_; x++) {
        std::size_t index = y * config_.grid_x_ + x;
        // Create tile
        if (config_.map_[index] != 0) {
          map_[index] = Tile{static_cast<double>(x) * config_.tile_, 
                             static_cast<double>(y) * config_.tile_, 
                             config_.tile_, 
                             config_.tile_,
                             TileType::Solid};
        }
      }
    }
  }

  void World::GameLoop()
  {
    const double tick_seconds = 1.0 / config_.tick_rate_;
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
    // Double buffering incoming queue.
    TSSwap(game_subsystem_->in_queue_, net_subsystem_->in_queue_);

    spdlog::info("queue length: {}", game_subsystem_->in_queue_.Size());

    spdlog::info("number of players: {}", players_.size());
    for (auto item : players_) {
      item.second->SetVel(0.0, item.second->GetVelY());
    }

    // Handle player inputs
    while (!game_subsystem_->in_queue_.Empty()) {
      auto packet = game_subsystem_->in_queue_.TryPop();
      if (packet)
        ProcessInput(std::move(packet.value()), dt);
    }

    // Update physics
    spdlog::info("number of players: {}", players_.size());
    for (auto item : players_)
      Update(*item.second, dt);

    // Push all packets to network queue for broadcast.
    while (!game_subsystem_->out_queue_.Empty()) {
      auto packet = game_subsystem_->out_queue_.TryPop();
      net_subsystem_->out_queue_.Push(std::move(packet.value()));
    }
  }

  void World::ProcessInput(std::unique_ptr<ServerPacket> packet, double dt)
  {
    spdlog::info("(World::ProcessInput)");
    // TODO check is this player exists
    auto player = players_[packet->GetID()];
    if (!player)
      spdlog::info("player not found id: {}\nfile: {} line: {}", packet->GetID(), __FILE__, __LINE__);
    auto net_packet = packet->GetNetPacket();
    std::uint16_t opcode = net_packet.GetHeadOpcode();

    double speed = 100.0 * dt;
    double jump_force = 400.0 * dt;

    spdlog::info("Process packet:\nid: {}\nopcode:{}", packet->GetID(), net_packet.GetHeadOpcode());

    switch (to_opcode(opcode)) {
      case Opcodes::CreatePlayer:
      {
        spdlog::info("Opcodes::CreatePlayer");
        auto player  = std::make_shared<Player>(packet->GetID(), 
                                                config_.player_.player_start_x_ * config_.tile_ + config_.player_.player_offset_, 
                                                config_.player_.player_start_y_ * config_.tile_ + config_.player_.player_offset_,
                                                0.0, 0.0,
                                                config_.player_.width_,
                                                config_.player_.height_);
        AddPlayer(player);
        break;
      }
      case Opcodes::RemovePlayer:
        spdlog::info("Opcodes::RemovePlayer");
        RemovePlayer(packet->GetID());
        break;
      case Opcodes::MoveLeft:
        spdlog::info("Opcodes::MoveLeft");
        player->SetVel(-speed, player->GetVelY());
        break;
      case Opcodes::MoveRight:
        spdlog::info("Opcodes::MoveRight");
        player->SetVel(speed, player->GetVelY());
        break;
      case Opcodes::Jump:
        spdlog::info("Opcodes::Jump");
        if (player->OnGround()) {
          player->SetVel(player->GetVelX(), -jump_force);
          player->SetOnGround(false);
          spdlog::info("JUMP!");
        }
        break;
      default:
        spdlog::warn("unknown opcode: {}", opcode);
    }
  }

  void World::Update(IPlayer& player, double dt)
  {
    spdlog::info("(World::Update)");
    const double g = 9.8;
    double vel_y = player.GetVelY() + g * dt;
    player.SetVel(player.GetVelX(), vel_y);
    MovePlayer(player);
    // spdlog::info("vel_y: {}", player.GetVelY());

    spdlog::info("make move packet");
    auto move_packet = std::make_unique<ServerPacket>(MovePlayerPacket(player.GetID(), player.GetX(), player.GetY()), player.GetID());
    game_subsystem_->out_queue_.Push(std::move(move_packet));
  }

  void World::MovePlayer(IPlayer& player)
  {
    spdlog::info("(World::MovePlayer)");
    double vel_x = player.GetVelX();
    double vel_y = player.GetVelY();

    /* ------ X Axis ------*/
    if (vel_x != 0 ) {
      spdlog::info("============== X AXIS ==============");
      // calculate collision along x axis
      SweptData swept = collision_.SweptAxis(player, 
                                             config_.tile_, config_.grid_x_, config_.grid_y_,
                                             map_,
                                             vel_x, 0.0);
    
      vel_x *= swept.entry_time_;
      player.Move(vel_x, 0.0);
      if (swept.hit_) {
        player.SetVel(0.0, vel_y);
      // spdlog::info("HIT SIDE WALL\n"\
      //              "x: {} y: {}", player.GetX(), player.GetY());
      }
      spdlog::info("============== X AXIS ==============");
    }

    /* ------ Y Axis ------*/
    if (vel_y != 0 ) {
      spdlog::info("============== Y AXIS ==============");
      // calculate collision along y axis
      SweptData swept = collision_.SweptAxis(player, 
                                             config_.tile_, config_.grid_x_, config_.grid_y_,
                                             map_,
                                             0.0, vel_y);
    
      vel_y *= swept.entry_time_;
      player.Move(0.0, vel_y);
      player.SetOnGround(false);
      if (swept.hit_) {
        if (vel_y >= 0.0) {
          player.SetOnGround(true);
          // spdlog::info("HIT GROUND");
        } else {
          // spdlog::info("HIT WALL");
        }
        player.SetVel(vel_x, 0.0);
      }
      spdlog::info("============== Y AXIS ==============");
    }
  }

  void World::AddPlayer(std::shared_ptr<IPlayer> player)
  {
    spdlog::info("(World::AddPlayer)");
    {
      std::lock_guard lock(players_mutex_);
      // TODO check if this id already exists
      // Otherwise it overwrite previous player
      players_[player->GetID()] = player;

      spdlog::info("CreatePlayerPacket:\nid: {}\nx: {}\ny: {}\nwidth: {}\nheight: {}", 
                   player->GetID(), 
                   player->GetX(), 
                   player->GetY(),
                   player->GetWidth(),
                   player->GetHeight());
      // Create player on client side
      auto create_packet = std::make_unique<ServerPacket>(CreatePlayerPacket(
        player->GetID(),
        player->GetX(),
        player->GetY(),
        player->GetWidth(),
        player->GetHeight()
      ), player->GetID(), PacketType::Rpc);

      // spdlog::info("packet body size: {}", packet0.GetBodySize());
      // TODO make it instance send
      game_subsystem_->out_queue_.Push(std::move(create_packet));

      // Send all players to new player
      NetPacket spawn_packet;
      spawn_packet.SetHeadOpcode(to_uint16(Opcodes::SpawnPlayers));
      spawn_packet << players_.size() - 1;
      for (const auto& elem : players_) {
        if (elem.first != player->GetID()) {
          spdlog::info("make spawn_packet id: {} x: {} y: {} width: {} height: {}", 
                       elem.second->GetID(), 
                       elem.second->GetX(), 
                       elem.second->GetY(),
                       elem.second->GetWidth(),
                       elem.second->GetHeight());

          spawn_packet << elem.second->GetID() 
                  << elem.second->GetX() 
                  << elem.second->GetY()
                  << elem.second->GetWidth()
                  << elem.second->GetHeight();
        }
      }
      game_subsystem_->out_queue_.Push(std::make_unique<ServerPacket>(std::move(spawn_packet), player->GetID(), PacketType::Rpc));
    }
    
    // // Notify others
    auto add_packet = std::make_unique<ServerPacket>(AddPlayerPacket(
      player->GetID(), 
      player->GetX(), 
      player->GetY(), 
      player->GetWidth(), 
      player->GetHeight()
    ), player->GetID(), PacketType::RpcOthers);
    game_subsystem_->out_queue_.Push(std::move(add_packet));
  }

  void World::RemovePlayer(std::size_t id)
  {
    spdlog::info("(World::RemovePlayer)");
    std::lock_guard lock(players_mutex_);
    // TODO check if this id is exists
    players_.erase(id);
    auto remove_packet = std::make_unique<ServerPacket>(RemovePlayerPacket(id), id);
    game_subsystem_->out_queue_.Push(std::move(remove_packet));
  }

  std::size_t World::PlayerNumbers() const
  {
    std::lock_guard lock(players_mutex_);
    return players_.size(); 
  }
}
