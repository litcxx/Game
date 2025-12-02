#include "collision.hpp"

#include <cstdint>
#include <limits>
#include <cmath>

namespace ep::game
{
  SweptData Collision::SweptAABB(const IBox& box1, const IBox& box2, double vel_x, double vel_y)
  {
    if (vel_x == 0 && vel_y == 0) {
      return {0, 0, 1.0};
    }

    double x_inv_entry, x_inv_exit;
    double y_inv_entry, y_inv_exit;

    // X axis
    if (vel_x > 0.0) {
      x_inv_entry = box2.GetX() - (box1.GetX() + box1.GetWidth());
      x_inv_exit = (box2.GetX() + box2.GetWidth()) - box1.GetX();
    } else if (vel_x < 0.0) {
      x_inv_entry = (box2.GetX() + box2.GetWidth()) - box1.GetX();
      x_inv_exit = box2.GetX() - (box1.GetX() + box1.GetWidth());
    } else {
      x_inv_entry = -std::numeric_limits<double>::infinity();
      x_inv_exit = std::numeric_limits<double>::infinity();
    }

    // Y axis
    if (vel_y > 0.0) {
      y_inv_entry = box2.GetY() - (box1.GetY() + box1.GetHeight());
      y_inv_exit = (box2.GetY() + box2.GetHeight()) - box1.GetY();
    } else if (vel_y < 0.0) {
      y_inv_entry = (box2.GetY() + box2.GetHeight()) - box1.GetHeight();
      y_inv_exit = box2.GetY() - (box1.GetY() + box1.GetHeight());
    } else {
      y_inv_entry = -std::numeric_limits<double>::infinity();
      y_inv_exit = std::numeric_limits<double>::infinity();
    }

    // x entry and exit time
    double x_entry = vel_x == 0.0 ? x_inv_entry : x_inv_entry / vel_x;
    double x_exit = vel_x == 0.0 ? x_inv_exit : x_inv_exit / vel_x;

    // y entry and exit time
    double y_entry = vel_y == 0.0 ? y_inv_entry : y_inv_entry / vel_y;
    double y_exit = vel_y == 0.0 ? y_inv_exit : y_inv_exit / vel_y;

    // Find minimum entry and exit time on both axes
    double entry_time = std::fmax(x_entry, y_entry);
    double exit_time = std::fmin(x_exit, y_exit);

    // No collision ?
    if (entry_time > exit_time || entry_time < 0.0 || exit_time > 1.0) {
      return {0, 0, 1.0};
    }

    std::uint8_t normal_x = 0;
    std::uint8_t normal_y = 0;
    if (x_entry > y_entry) {
      normal_x = vel_x > 0 ? -1 : 1;
    } else {
      normal_y = vel_y > 0 ? -1 : 1;
    }
    
    return {normal_x, normal_y, entry_time};
  }
}
