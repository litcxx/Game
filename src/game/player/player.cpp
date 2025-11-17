#include "player.hpp"

namespace ep::game
{
  Player::Player(double x, double y, double z, std::size_t id) :
    x_(x),
    y_(y),
    z_(z),
    id_(id)
  {}

  void Player::Move(double dx, double dy, double dz)
  {
    // TODO check collision
    x_ += dx;
    y_ += dy;
    z_ += dz;
  }
}
