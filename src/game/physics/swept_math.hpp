#pragma once

#include <cstdint>

#include <tile/i_box.hpp>

namespace ep::game
{
  struct SweptData {
    double entry_time_;
    std::uint8_t normal_x_;
    std::uint8_t normal_y_;
    bool hit_;
  };

  SweptData SweptAABB(const IBox& box1, const IBox& box2, double vel_x, double vel_y) noexcept;
}
