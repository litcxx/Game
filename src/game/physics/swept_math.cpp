#include "swept_math.hpp"

#include <limits>

#include <spdlog/spdlog.h>

namespace ep::game
{
  SweptData SweptAABB(const IBox& box1, const IBox& box2, double vel_x, double vel_y) noexcept
  {
    spdlog::info("(Collision::SweptAABB)");
    if (vel_x == 0 && vel_y == 0)
      return {1.0, 0, 0, false};

    double x_inv_entry, x_inv_exit;
    double y_inv_entry, y_inv_exit;

    spdlog::info("INIT DATA:\n" \
                 "box1.x: {} box1.y: {}\n" \
                 "box1.width: {} box1.height: {}\n" \
                 "box2.x: {} box2.y: {}\n" \
                 "box2.width: {} box2.height: {}\n" \
                 "vel_x: {} vel_y: {}",
                 box1.GetX(), box1.GetY(), box1.GetWidth(), box1.GetHeight(),
                 box2.GetX(), box2.GetY(), box2.GetWidth(), box2.GetHeight(), vel_x, vel_y);

    // X axis
    if (vel_x > 0.0) {
      x_inv_entry = box2.GetX() - (box1.GetX() + box1.GetWidth());
      x_inv_exit = (box2.GetX() + box2.GetWidth()) - box1.GetX();
    } else if (vel_x < 0.0) {
      x_inv_entry = (box2.GetX() + box2.GetWidth()) - box1.GetX();
      x_inv_exit = box2.GetX() - (box1.GetX() + box1.GetWidth());
    } else {
      // vel_x == 0
      // If box1 does not overlap with box 2 along X axis
      // then there is no collision
      if ((box1.GetX() + box1.GetWidth()) <= box2.GetX() || box1.GetX() >= (box2.GetX() + box2.GetWidth())) {
        spdlog::info("VEL X == 0 and X coords does not overlap, NO COLLISION ");
        return {1.0, 0, 0, false};
      }

      x_inv_entry = -std::numeric_limits<double>::infinity();
      x_inv_exit = std::numeric_limits<double>::infinity();
    }

    spdlog::info("X AXIS:\n" \
                 "x_inv_entry: {}\nx_inv_exit: {}", x_inv_entry, x_inv_exit);

    // Y axis
    if (vel_y > 0.0) {
      y_inv_entry = box2.GetY() - (box1.GetY() + box1.GetHeight());
      y_inv_exit = (box2.GetY() + box2.GetHeight()) - box1.GetY();
    } else if (vel_y < 0.0) {
      y_inv_entry = (box2.GetY() + box2.GetHeight()) - box1.GetY();
      y_inv_exit = box2.GetY() - (box1.GetY() + box1.GetHeight());
    } else {
      // vel_y == 0
      // If box1 does not overlap with box 2 along Y axis
      // then there is no collision
      if ((box1.GetY() + box1.GetHeight()) <= box2.GetY() || box1.GetY() >= (box2.GetY() + box2.GetHeight())) {
        spdlog::info("VEL Y == 0 and Y coords does not overlap, NO COLLISION ");
        return {1.0, 0, 0, false};
      }

      y_inv_entry = -std::numeric_limits<double>::infinity();
      y_inv_exit = std::numeric_limits<double>::infinity();
    }

    spdlog::info("Y AXIS:\n" \
                 "y_inv_entry: {}\ny_inv_exit: {}", y_inv_entry, y_inv_exit);

    // x entry and exit time
    double x_entry = vel_x == 0.0 ? x_inv_entry : x_inv_entry / vel_x;
    double x_exit = vel_x == 0.0 ? x_inv_exit : x_inv_exit / vel_x;

    spdlog::info("X TIME:\n" \
                 "x_entry: {}\nx_exit: {}", 
                 x_entry, x_exit);

    // y entry and exit time
    double y_entry = vel_y == 0.0 ? y_inv_entry : y_inv_entry / vel_y;
    double y_exit = vel_y == 0.0 ? y_inv_exit : y_inv_exit / vel_y;

    spdlog::info("Y TIME:\n" \
                 "y_entry: {}\ny_exit: {}", y_entry, y_exit);

    // Find minimum entry and exit time on both axes
    double entry_time = std::fmax(x_entry, y_entry);
    double exit_time = std::fmin(x_exit, y_exit);

    spdlog::info("OVERALL TIME:\n" \
                 "entry_time: {}\nexit_time: {}", entry_time, exit_time);

    // No collision ?
    if (entry_time > exit_time || entry_time < 0.0 || entry_time > 1.0) {
      spdlog::info("NO COLLISION");
      return {1.0, 0, 0, false};
    }

    spdlog::info("COLLISION");

    std::uint8_t normal_x = 0;
    std::uint8_t normal_y = 0;
    if (x_entry > y_entry) {
      normal_x = vel_x > 0 ? -1 : 1;
    } else {
      normal_y = vel_y > 0 ? -1 : 1;
    }
    
    spdlog::info("NORMALS:\n"\
                 "normal_x: {}\nnormal_y: {}", normal_x, normal_y);
    
    return {entry_time, normal_x, normal_y, true};
  }
}
