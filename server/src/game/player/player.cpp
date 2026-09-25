#include "player.hpp"

namespace lit::game {
Player::Player(std::size_t id, double x, double y, bool alive) noexcept
    : id_{id}, x_{x}, y_{y}, alive_{alive} {}

void Player::move(double dx, double dy) noexcept {
    x_ += dx;
    y_ += dy;
}
}  // namespace lit::game
