#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>

#include "protocol/network_subsystem.hpp"
#include "protocol/game_subsystem.hpp"
#include "player/i_player.hpp"
#include "protocol/events.hpp"

namespace ep::game
{
  class World {
  public:
    explicit World(std::shared_ptr<net::NetworkSubsystem> net_subsystem, std::shared_ptr<game::GameSubsystem> game_subsystem, std::uint8_t tick_rate);
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    ~World() = default;

    void GameLoop();
    void AddPlayer(std::shared_ptr<IPlayer> player);
    void RemovePlayer(std::size_t id);
    std::size_t PlayerNumbers() const;
  private:
    void Tick(double dt);
    void ProcessEvent(const Event& event);
    void ProcessInput(net::NetPacket packet);

    void OpcodeMovePlayer(net::NetPacket& packet, std::shared_ptr<IPlayer> player);

    std::shared_ptr<net::NetworkSubsystem> net_subsystem_;
    std::shared_ptr<game::GameSubsystem> game_subsystem_;
    std::uint8_t tick_rate_;
    mutable std::mutex players_mutex_;
    std::unordered_map<std::size_t, std::shared_ptr<IPlayer>> players_;
  };
}
