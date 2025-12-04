#pragma once

#include "i_player.hpp"

namespace ep::game
{
  class Player : public IPlayer {
  public:
    explicit Player(std::size_t id, double x, double y, std::uint8_t width, std::uint8_t height);
    ~Player() = default;

    double GetX() const noexcept override { return x_; }
    double GetY() const noexcept override { return y_; }
    std::uint8_t GetWidth() const noexcept override { return width_; };
    std::uint8_t GetHeight() const noexcept override { return height_; };

    std::size_t GetID() const noexcept override { return id_; }

    void Move(double dx, double dy) override;

  private:
    std::size_t id_;
    double x_;
    double y_;
    std::uint8_t width_;
    std::uint8_t height_;
  };
}
