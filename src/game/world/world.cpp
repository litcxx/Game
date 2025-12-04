#include "world.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <thread>
#include <algorithm>

#include <spdlog/spdlog.h>

#include "config/config.hpp"
#include "physics/collision.hpp"
#include "player/i_player.hpp"
#include "protocol/base_packet.hpp"
#include "protocol/events.hpp"
#include "protocol/opcodes.hpp"
#include "tile/tile.hpp"
#include "utils/ts_queue.hpp"
#include "player/player.hpp"

namespace ep::game
{
  World::World(std::shared_ptr<ep::NetworkSubsystem> net_subsystem, 
               std::shared_ptr<ep::GameSubsystem> game_subsystem, 
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
    // Double buffering event queue.
    ep::TSSwap(game_subsystem_->event_queue_, net_subsystem_->event_queue_);

    // Double buffering incoming queue.
    ep::TSSwap(game_subsystem_->in_queue_, net_subsystem_->in_queue_);

    // Handle events.
    while (!game_subsystem_->event_queue_.Empty()) {
      auto event = game_subsystem_->event_queue_.TryPop();
      ProcessEvent(*event);
    }

    // Pop packet from queue and process it.
    while (!game_subsystem_->in_queue_.Empty()) {
      auto packet = game_subsystem_->in_queue_.TryPop();
      ProcessInput(std::move(*packet), dt);
    }

    // Push all packets to network queue for broadcast.
    while (!game_subsystem_->out_queue_.Empty()) {
      auto packet = game_subsystem_->out_queue_.TryPop();
      net_subsystem_->out_queue_.Push(std::move(packet));
    }
  }

  void World::ProcessEvent(const Event& event)
  {
    switch (event.GetEvent()) {
      case ep::EventCode::AddNewPlayer: {
        spdlog::info("game event: AddNewPlayer id: {}", event.GetID());
        auto player  = std::make_shared<Player>(event.GetID(), 
                                                config_.player_start_x_ * config_.tile_ + config_.player_offset_, 
                                                config_.player_start_y_ * config_.tile_ + config_.player_offset_, 
                                                config_.tile_ - config_.player_offset_ * 2,
                                                config_.tile_ - config_.player_offset_ * 2);
        AddPlayer(player);
        break;
      }
      case ep::EventCode::RemovePlayer: {
        spdlog::info("game event: remove player");
        RemovePlayer(event.GetID());
        break;
      }
      default:
        spdlog::info("game event: unknown event");
    }
  }

  void World::ProcessInput(ep::NetPacket packet, double dt)
  {
    ep::NetPacket send_packet;
    send_packet.SetPacketType(ep::PacketType::Broadcast);
    // TODO check is this player exists
    auto player = players_[packet.GetID()];
    std::uint16_t opcode = packet.GetHeadOpcode();

    double speed = 200.0 * dt;
    double vel_x = 0.0;
    double vel_y = 0.0;

    switch (to_opcode(opcode)) {
      case Opcodes::MoveForward:
        spdlog::info("Opcode::MoveForward");
        vel_y = -speed;
        MovePlayer(*player, vel_x, vel_y);
        OpcodeMovePlayer(send_packet, *player);
        break;
      case Opcodes::MoveLeft:
        spdlog::info("Opcodes::MoveLeft");
        vel_x = -speed;
        MovePlayer(*player, vel_x, vel_y);
        OpcodeMovePlayer(send_packet, *player);
        break;
      case Opcodes::MoveBackward:
        spdlog::info("Opcodes::MoveBackward");
        vel_y = speed;
        MovePlayer(*player, vel_x, vel_y);
        OpcodeMovePlayer(send_packet, *player);
        break;
      case Opcodes::MoveRight:
        spdlog::info("Opcodes::MoveRight");
        vel_x = speed;
        MovePlayer(*player, vel_x, vel_y);
        OpcodeMovePlayer(send_packet, *player);
        break;
      default:
        spdlog::warn("unknown opcode: {}", opcode);
    }
    
    game_subsystem_->out_queue_.Push(std::move(send_packet));
  }

  void World::MovePlayer(IPlayer& player, double vel_x, double vel_y)
  {
    spdlog::info("World::MovePlayer");

    // Find colliders and calculate minimum sweptAABB collision
    SweptData collision_res{1.0, 0, 0};
    auto indices = FindCollisionIndices(player, vel_x, vel_y);
    spdlog::info("indices: {}", indices.size());
    for (auto i : indices) {
      if (map_[i].GetType() != TileType::Empty) {
        spdlog::info("calculate sweptAABB for tile: {}", i);
        SweptData tmp = Collision::SweptAABB(player, map_[i], vel_x, vel_y);
        spdlog::info("sweptAABB time: {}", tmp.time_);
        if (collision_res.time_ > tmp.time_)
          collision_res = tmp;
      } else {
        spdlog::info("tile {} is empty", i);
      }
    }

    spdlog::info("time: {}", collision_res.time_);

    // Correct velocity
    vel_x *= collision_res.time_;
    vel_y *= collision_res.time_;

    spdlog::info("old player pos:\nx: {}\ny: {}", player.GetX(), player.GetY());

    player.Move(vel_x, vel_y);

    spdlog::info("new player pos:\nx: {}\ny: {}", player.GetX(), player.GetY());
  }

  void World::OpcodeMovePlayer(ep::NetPacket& packet, const IPlayer& player)
  {
    packet.SetHeadOpcode(to_uint16(ep::Opcodes::MovePlayer));
    spdlog::info("id: {} x: {} y: {}", player.GetID(), player.GetX(), player.GetY());
    packet << player.GetID() << player.GetX() << player.GetY();
    // spdlog::info("head size: {}", packet.GetHeadSize());
    // spdlog::info("payload size: {}", packet.GetBodySize());
  }

  std::set<std::size_t> World::FindCollisionIndices(const IPlayer& player, double vel_x, double vel_y)
  {
    spdlog::info("World::FindCollisionIndices");
    spdlog::info("vel: \nvel_x: {}\nvel_y: {}", vel_x, vel_y);
    if (vel_x == 0 && vel_y == 0) 
      return {};

    // Find start pos
    const double start_left = player.GetX();
    const double start_top = player.GetY();
    const double start_right = player.GetX() + player.GetWidth();
    const double start_bottom = player.GetY() + player.GetHeight();
  
    spdlog::info("start pos:\nstart_left: {}\nstart_top: {}\nstart_right: {}\nstart_bottom: {}",start_left, start_top, start_right, start_bottom);

    // Find next pos
    const double end_left = start_left + vel_x;
    const double end_top = start_top + vel_y;
    const double end_right = start_right + vel_x;
    const double end_bottom = start_bottom + vel_y;

    spdlog::info("end pos:\nend_left: {}\nend_top: {}\nend_right: {}\nend_bottom: {}",end_left, end_top, end_right, end_bottom);

    // Calculate bounding box
    double bb_left = std::floor(std::fmin(start_left, end_left) / config_.tile_);
    double bb_top = std::floor(std::fmin(start_top, end_top) / config_.tile_);
    double bb_right = std::floor(std::fmax(start_right, end_right) / config_.tile_);
    double bb_bottom  = std::floor(std::fmax(start_bottom, end_bottom) / config_.tile_);

    spdlog::info("bb pos:\nbb_left: {}\nbb_top: {}\nbb_right: {}\nbb_bottom: {}",bb_left, bb_top, bb_right, bb_bottom);

    // Clamp bounding box
    std::size_t tile_left = std::max<std::size_t>(0, bb_left);
    std::size_t tile_top = std::max<std::size_t>(0, bb_top);
    std::size_t tile_right = std::min<std::size_t>(config_.grid_x_ - 1, bb_right);
    std::size_t tile_bottom = std::min<std::size_t>(config_.grid_y_ - 1, bb_bottom);

    spdlog::info("tile pos:\ntile_left: {}\ntile_top: {}\ntile_right: {}\ntile_bottom: {}",tile_left, tile_top, tile_right, tile_bottom);

    std::set<std::size_t> res;
    // Push all tiles inside bounding box to colliders buffer
    for (std::size_t y = tile_top; y <= tile_bottom; y++) {
      for (std::size_t x = tile_left; x <= tile_right; x++) {
        std::size_t index = y * config_.grid_x_ + x;
        res.insert(index);
        // spdlog::info("+");
      }
    }

    // debug
    for (auto e : res)
      spdlog::info("index: {}", e);

    return std::move(res);
  }

  void World::AddPlayer(std::shared_ptr<IPlayer> player)
  {
    spdlog::info("World::AddPlayer");
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
      ep::NetPacket packet0 = CreatePlayerPacket(
        player->GetID(),
        player->GetX(),
        player->GetY(),
        player->GetWidth(),
        player->GetHeight()
      );

      // spdlog::info("packet body size: {}", packet0.GetBodySize());
      // TODO make it instance send
      game_subsystem_->out_queue_.Push(std::move(packet0));

      // Send all players to new player
      ep::NetPacket packet1;
      packet1.SetID(player->GetID());
      packet1.SetPacketType(ep::PacketType::Rpc);
      packet1.SetHeadOpcode(to_uint16(Opcodes::SpawnPlayers));
      packet1 << players_.size() - 1;
      for (const auto& elem : players_) {
        if (elem.first != player->GetID()) {
          spdlog::info("make packet1 id: {} x: {} y: {} width: {} height: {}", 
                       elem.second->GetID(), 
                       elem.second->GetX(), 
                       elem.second->GetY(),
                       elem.second->GetWidth(),
                       elem.second->GetHeight());

          packet1 << elem.second->GetID() 
                  << elem.second->GetX() 
                  << elem.second->GetY()
                  << elem.second->GetWidth()
                  << elem.second->GetHeight();
        }
      }
      game_subsystem_->out_queue_.Push(std::move(packet1));
    }
    
    // // Notify others
    ep::NetPacket packet2 = AddPlayerPacket(player->GetID(), player->GetX(), player->GetY(), player->GetWidth(), player->GetHeight());
    game_subsystem_->out_queue_.Push(std::move(packet2));
  }

  void World::RemovePlayer(std::size_t id)
  {
    spdlog::info("World::RemovePlayer");
    std::lock_guard lock(players_mutex_);
    // TODO check if this id is exists
    players_.erase(id);
    ep::NetPacket packet = RmvPlayerPacket(id);
    game_subsystem_->out_queue_.Push(std::move(packet));
  }

  std::size_t World::PlayerNumbers() const
  {
    std::lock_guard lock(players_mutex_);
    return players_.size(); 
  }
}
