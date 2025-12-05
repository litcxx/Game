#include "collision.hpp"

#include <cstdint>
#include <limits>
#include <cmath>

#include <spdlog/spdlog.h>

namespace ep::game
{
  SweptData Collision::SweptAxis(const IBox &box, 
                                 std::uint8_t tile, std::uint16_t grid_x, std::uint8_t grid_y,
                                 const std::vector<Tile> &map, 
                                 double vel_x, double vel_y)
  {
    SweptData res{1.0, 0, 0, false};
    auto indices = FindCollisionIndices(box, tile, grid_x, grid_y, vel_x, vel_y);
    // spdlog::info("FindCollisionIndices: {}", indices.size());

    for (auto i : indices) {
      if (map[i].GetType() != TileType::Empty) {
        // spdlog::info("calculate sweptAABB for tile: {}", i);
        SweptData tmp = SweptAABB(box, map[i], vel_x, vel_y);
        // spdlog::info("entry_time: {}", tmp.entry_time_);
        if (res.entry_time_ > tmp.entry_time_)
          res = tmp;
      } else {
        // spdlog::info("tile {} is empty", i);
      }
    }
    
    return res;
  }

  std::set<std::size_t> Collision::FindCollisionIndices(const IBox& box, 
                                                        uint8_t tile, std::uint16_t grid_x, std::uint8_t grid_y, 
                                                        double vel_x, double vel_y)
  {
    // spdlog::info("World::FindCollisionIndices");
    // spdlog::info("vel: \nvel_x: {}\nvel_y: {}", vel_x, vel_y);
    if (vel_x == 0 && vel_y == 0) 
      return {};

    // Find start pos
    const double start_left = box.GetX();
    const double start_top = box.GetY();
    const double start_right = box.GetX() + box.GetWidth();
    const double start_bottom = box.GetY() + box.GetHeight();
  
    // spdlog::info("start pos:\nstart_left: {}\nstart_top: {}\nstart_right: {}\nstart_bottom: {}",start_left, start_top, start_right, start_bottom);

    // Find next pos
    const double end_left = start_left + vel_x;
    const double end_top = start_top + vel_y;
    const double end_right = start_right + vel_x;
    const double end_bottom = start_bottom + vel_y;

    // spdlog::info("end pos:\nend_left: {}\nend_top: {}\nend_right: {}\nend_bottom: {}",end_left, end_top, end_right, end_bottom);

    // Calculate bounding box
    double bb_left = std::floor(std::fmin(start_left, end_left) / tile);
    double bb_top = std::floor(std::fmin(start_top, end_top) / tile);
    double bb_right = std::floor(std::fmax(start_right, end_right) / tile);
    double bb_bottom  = std::floor(std::fmax(start_bottom, end_bottom) / tile);

    // spdlog::info("bb pos:\nbb_left: {}\nbb_top: {}\nbb_right: {}\nbb_bottom: {}",bb_left, bb_top, bb_right, bb_bottom);

    // Clamp bounding box
    std::size_t tile_left = std::max<std::size_t>(0, bb_left);
    std::size_t tile_top = std::max<std::size_t>(0, bb_top);
    std::size_t tile_right = std::min<std::size_t>(grid_x - 1, bb_right);
    std::size_t tile_bottom = std::min<std::size_t>(grid_y - 1, bb_bottom);

    // spdlog::info("tile pos:\ntile_left: {}\ntile_top: {}\ntile_right: {}\ntile_bottom: {}",tile_left, tile_top, tile_right, tile_bottom);

    std::set<std::size_t> res;
    // Push all tiles inside bounding box to colliders buffer
    for (std::size_t y = tile_top; y <= tile_bottom; y++) {
      for (std::size_t x = tile_left; x <= tile_right; x++) {
        std::size_t index = y * grid_x + x;
        res.insert(index);
        // spdlog::info("+");
      }
    }

    // debug
    // for (auto e : res)
    //   spdlog::info("index: {}", e);

    return std::move(res);
  }

  SweptData Collision::SweptAABB(const IBox& box1, const IBox& box2, double vel_x, double vel_y) const noexcept
  {
    // spdlog::info("Collision::SweptAABB");
    if (vel_x == 0 && vel_y == 0)
      return {1.0, 0, 0, false};

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
      // vel_x == 0
      // If box1 does not overlap with box 2 along X axis
      // then there is no collision
      if ((box1.GetX() + box1.GetWidth()) <= box2.GetX() || (box2.GetX() + box2.GetWidth()) <= box1.GetX())
        return {1.0, 0, 0, false};

      x_inv_entry = -std::numeric_limits<double>::infinity();
      x_inv_exit = std::numeric_limits<double>::infinity();
    }

    // spdlog::info("x_inv:\nx_inv_entry: {}\nx_inv_exit: {}", x_inv_entry, x_inv_exit);

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
      if ((box1.GetY() + box1.GetHeight()) <= box2.GetHeight() || (box2.GetY() + box2.GetHeight()) <= box1.GetY())
        return {1.0, 0, 0, false};

      y_inv_entry = -std::numeric_limits<double>::infinity();
      y_inv_exit = std::numeric_limits<double>::infinity();
    }

    // spdlog::info("y_inv:\ny_inv_entry: {}\ny_inv_exit: {}", y_inv_entry, y_inv_exit);

    // x entry and exit time
    double x_entry = vel_x == 0.0 ? x_inv_entry : x_inv_entry / vel_x;
    double x_exit = vel_x == 0.0 ? x_inv_exit : x_inv_exit / vel_x;

    // spdlog::info("x_time:\nx_entry: {}\nx_exit: {}", x_entry, x_exit);

    // y entry and exit time
    double y_entry = vel_y == 0.0 ? y_inv_entry : y_inv_entry / vel_y;
    double y_exit = vel_y == 0.0 ? y_inv_exit : y_inv_exit / vel_y;

    // spdlog::info("y_time:\ny_entry: {}\ny_exit: {}", y_entry, y_exit);

    // Find minimum entry and exit time on both axes
    double entry_time = std::fmax(x_entry, y_entry);
    double exit_time = std::fmin(x_exit, y_exit);

    // spdlog::info("time:\nentry_time: {}\nexit_time: {}", entry_time, exit_time);

    // No collision ?
    if (entry_time > exit_time || entry_time < 0.0 || entry_time > 1.0) {
      // spdlog::info("no collision");
      return {1.0, 0, 0, false};
    }

    // spdlog::info("collision");

    std::uint8_t normal_x = 0;
    std::uint8_t normal_y = 0;
    if (x_entry > y_entry) {
      normal_x = vel_x > 0 ? -1 : 1;
    } else {
      normal_y = vel_y > 0 ? -1 : 1;
    }
    
    return {entry_time, normal_x, normal_y, true};
  }
}
