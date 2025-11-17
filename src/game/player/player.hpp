#pragma once

#include <memory>
#include <cstddef>

#include "i_player.hpp"

namespace ep::game
{
  class Player : IPlayer {
  public:
    explicit Player(double x, double y, double z, std::size_t id);
    ~Player() = default;

    void Move(double dx, double dy, double dz) override;

    double GetX() const noexcept override { return x_; }
    double GetY() const noexcept override { return y_; }
    double GetZ() const noexcept override { return z_; }
    std::size_t GetID() const noexcept override { return id_; }

  private:
    // Player's position in the world
    double x_;
    double y_;
    double z_;

    std::size_t id_;
  };
}
