#include "player.hpp"

namespace ep::game
{
  Player::Player(std::size_t id, double x, double y, double vel_x, double vel_y, std::uint8_t width, std::uint8_t height) :
    id_(id),
    x_(x),
    y_(y),
    vel_x_(vel_x),
    vel_y_(vel_y),
    width_(width),
    height_(height)
  {}

  void Player::Move(double dx, double dy)
  {
    x_ += dx;
    y_ += dy;
  }
}
