#include "player.hpp"

namespace ep::game
{
  Player::Player(double x, double y, std::size_t id) :
    x_(x),
    y_(y),
    id_(id)
  {}

  void Player::Move(double dx, double dy)
  {
    // TODO check collision
    x_ += dx;
    y_ += dy;
  }
}
