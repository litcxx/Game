#pragma once

#include <memory>
#include <cstddef>

#include "i_player.hpp"

namespace ep::game
{
  class Player : IPlayer {
  public:
    explicit Player(float x, float y, float z, std::size_t id);
    ~Player() = default;

    void Move(float dx, float dy, float dz) override;

  private:
    // Player's position in the world
    float x_;
    float y_;
    float z_;

    std::size_t id_;
  };
}
