#pragma once

#include "i_player.hpp"

namespace ep::game
{
  class Player : public IPlayer {
  public:
    explicit Player(std::size_t id, double x, double y, double vel_x, double vel_y, std::uint8_t width, std::uint8_t height);
    ~Player() = default;

    double GetX() const noexcept override { return x_; }
    double GetY() const noexcept override { return y_; }
    std::uint8_t GetWidth() const noexcept override { return width_; };
    std::uint8_t GetHeight() const noexcept override { return height_; };

    std::size_t GetID() const noexcept override { return id_; }

    // velocity
    double GetVelX() const noexcept { return vel_x_; }
    double GetVelY() const noexcept { return vel_y_; }
    void SetVelX(double vel_x) noexcept { vel_x_ = vel_x; }
    void SetVelY(double vel_y) noexcept { vel_y_ = vel_y; }

    void Move(double dx, double dy) override;

  private:
    std::size_t id_;
    double x_;
    double y_;
    double vel_x_;
    double vel_y_;
    std::uint8_t width_;
    std::uint8_t height_;
  };
}
