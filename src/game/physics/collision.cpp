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

    spdlog::info("===============");
    int index = -1;
    for (auto i : indices)
      spdlog::info("index: {}", i);
    spdlog::info("===============");

    for (auto i : indices) {
      if (map[i].GetType() != TileType::Empty) {
        spdlog::info("calculate sweptAABB for tile: {}", i);
        SweptData tmp = SweptAABB(box, map[i], vel_x, vel_y);
        spdlog::info("entry_time: {}", tmp.entry_time_);
        if (res.entry_time_ > tmp.entry_time_) {
          res = tmp;
          index = i;
        }
      } else {
        spdlog::info("tile {} is empty", i);
      }
    }

    spdlog::info("hit with: {} tile", index);
    
    return res;
  }

  std::set<std::size_t> Collision::FindCollisionIndices(const IBox& box, 
                                                        uint8_t tile, std::uint16_t grid_x, std::uint8_t grid_y, 
                                                        double vel_x, double vel_y)
  {
    spdlog::info("World::FindCollisionIndices");
    spdlog::info("vel: \nvel_x: {}\nvel_y: {}", vel_x, vel_y);
    if (vel_x == 0 && vel_y == 0) 
      return {};

    // Find start pos
    const double start_left = box.GetX();
    const double start_top = box.GetY();
    const double start_right = box.GetX() + box.GetWidth();
    const double start_bottom = box.GetY() + box.GetHeight();
  
    spdlog::info("start pos:\nstart_left: {}\nstart_top: {}\nstart_right: {}\nstart_bottom: {}",start_left, start_top, start_right, start_bottom);

    // Find next pos
    const double end_left = start_left + vel_x;
    const double end_top = start_top + vel_y;
    const double end_right = start_right + vel_x;
    const double end_bottom = start_bottom + vel_y;

    spdlog::info("end pos:\nend_left: {}\nend_top: {}\nend_right: {}\nend_bottom: {}",end_left, end_top, end_right, end_bottom);

    // Calculate bounding box
    double bb_left = std::floor(std::fmin(start_left, end_left) / tile);
    double bb_top = std::floor(std::fmin(start_top, end_top) / tile);
    double bb_right = std::floor(std::fmax(start_right, end_right) / tile);
    double bb_bottom  = std::floor(std::fmax(start_bottom, end_bottom) / tile);

    spdlog::info("bb pos:\nbb_left: {}\nbb_top: {}\nbb_right: {}\nbb_bottom: {}",bb_left, bb_top, bb_right, bb_bottom);

    // Clamp bounding box
    std::size_t tile_left = std::max<std::size_t>(0, bb_left);
    std::size_t tile_top = std::max<std::size_t>(0, bb_top);
    std::size_t tile_right = std::min<std::size_t>(grid_x - 1, bb_right);
    std::size_t tile_bottom = std::min<std::size_t>(grid_y - 1, bb_bottom);

    spdlog::info("tile pos:\ntile_left: {}\ntile_top: {}\ntile_right: {}\ntile_bottom: {}",tile_left, tile_top, tile_right, tile_bottom);

    std::set<std::size_t> res;
    // Push all tiles inside bounding box to colliders buffer
    for (std::size_t y = tile_top; y <= tile_bottom; y++) {
      for (std::size_t x = tile_left; x <= tile_right; x++) {
        std::size_t index = y * grid_x + x;
        res.insert(index);
        // spdlog::info("+");
      }
    }

    return std::move(res);
  }
}
