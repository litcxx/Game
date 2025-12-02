#include "player.hpp"

namespace ep::game
{
  Player::Player(std::size_t id, double x, double y, std::uint8_t width, std::uint8_t height) :
    id_(id),
    x_(x),
    y_(y),
    width_(width),
    height_(height)
  {}

  void Player::Move(double x, double y)
  {
    x_ = x;
    y_ = y;
  }
}
