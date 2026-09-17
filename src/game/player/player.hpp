#pragma once

#include "i_player.hpp"

namespace lit::game {
class Player : public IPlayer {
  public:
    explicit Player(std::size_t id, double x, double y, double vel_x, double vel_y,
                    std::uint8_t width, std::uint8_t height);
    ~Player() = default;

    double get_x() const noexcept override { return x_; }
    double get_y() const noexcept override { return y_; }
    std::uint8_t get_width() const noexcept override { return width_; };
    std::uint8_t get_height() const noexcept override { return height_; };
    std::size_t get_id() const noexcept override { return id_; }

    void move(double dx, double dy) override;

    // velocity
    double get_vel_x() const noexcept override { return vel_x_; }
    double get_vel_y() const noexcept override { return vel_y_; }
    void set_vel(double vel_x, double vel_y) noexcept override {
        vel_x_ = vel_x;
        vel_y_ = vel_y;
    }

    bool on_ground() const noexcept override { return on_ground_; }
    void set_on_ground(bool state) noexcept override { on_ground_ = state; }

  private:
    std::size_t id_;
    double x_;
    double y_;
    double vel_x_;
    double vel_y_;
    std::uint8_t width_;
    std::uint8_t height_;
    bool on_ground_;
};
}  // namespace lit::game
