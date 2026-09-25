#pragma once

#include "i_player.hpp"

namespace lit::game {
class Player : public IPlayer {
  public:
    explicit Player(std::size_t id, double x, double y, bool alive) noexcept;
    ~Player() = default;

    double get_x() const noexcept override { return x_; }
    double get_y() const noexcept override { return y_; }
    std::size_t get_id() const noexcept override { return id_; }
    void move(double dx, double dy) noexcept override;

  private:
    std::size_t id_;
    double x_;
    double y_;
    bool alive_;
};
}  // namespace lit::game
