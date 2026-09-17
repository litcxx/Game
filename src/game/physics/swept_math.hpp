#pragma once

#include <cstdint>
#include <tile/i_box.hpp>

namespace ep::game {
struct SweptData {
    double entry_time;
    std::int8_t normal_x;
    std::int8_t normal_y;
    bool hit;
};

SweptData swept_aabb(const IBox& box1, const IBox& box2, double vel_x, double vel_y) noexcept;
}  // namespace ep::game
