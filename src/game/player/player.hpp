#pragma once

#include <cstddef>
#include <cstdint>

#include "i_player.hpp"

namespace ep::game
{
  class Player : public IPlayer {
  public:
    explicit Player(double x, double y, std::size_t id);
    ~Player() = default;

    void Move(double dx, double dy) override;

    double GetX() const noexcept override { return x_; }
    double GetY() const noexcept override { return y_; }
    std::size_t GetID() const noexcept override { return id_; }

  private:
    // Player's position in the world
    double x_;
    double y_;

    std::size_t id_;
  };
}
