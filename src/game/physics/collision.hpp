#pragma once

#include "tile/i_box.hpp"

namespace ep::game
{
  struct SweptData {
    std::uint8_t normal_x_;
    std::uint8_t normal_y_;
    double time_;
  };
  class Collision {
  public:
    SweptData SweptAABB(const IBox& box1, const IBox& box2, double vel_x, double vel_y);
  };
}
