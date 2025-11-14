#include "player.hpp"

namespace ep::game
{
  Player::Player(float x, float y, float z, std::size_t id) :
    x_(x),
    y_(y),
    z_(z),
    id_(id)
  {}

  void Player::Move(float dx, float dy, float dz)
  {
    // TODO check collision
    x_ += dx;
    y_ += dy;
    z_ += dz;
  }
}
