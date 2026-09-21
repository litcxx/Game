#include "collision.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace lit::game {
SweptData Collision::swept_axis(const IBox &box, std::uint8_t tile, std::uint16_t grid_x,
                                std::uint8_t grid_y, const std::vector<Tile> &map, double vel_x,
                                double vel_y) {
    SweptData res{1.0, 0, 0, false};
    auto indices = find_collision_indices(box, tile, grid_x, grid_y, vel_x, vel_y);
    // spdlog::info("FindCollisionIndices: {}", indices.size());

    // spdlog::info("===============");
    // int index = -1;
    // for (auto i : indices)
    //   spdlog::info("index: {}", i);
    // spdlog::info("===============");

    for (auto i : indices) {
        if (map[i].get_type() != TileType::Empty) {
            // spdlog::info("calculate sweptAABB for tile: {}", i);
            SweptData tmp = swept_aabb(box, map[i], vel_x, vel_y);
            // spdlog::info("entry_time: {}", tmp.entry_time_);
            if (res.entry_time > tmp.entry_time) {
                res = tmp;
                // index = i;
            }
        } else {
            // spdlog::info("tile {} is empty", i);
        }
    }

    // spdlog::info("hit with: {} tile", index);

    return res;
}

std::set<std::size_t> Collision::find_collision_indices(const IBox &box, uint8_t tile,
                                                        std::uint16_t grid_x, std::uint8_t grid_y,
                                                        double vel_x, double vel_y) {
    // spdlog::info("World::FindCollisionIndices");
    // spdlog::info("vel: \nvel_x: {}\nvel_y: {}", vel_x, vel_y);
    if (vel_x == 0 && vel_y == 0) return {};

    // Find start pos
    const double start_left = box.get_x();
    const double start_top = box.get_y();
    const double start_right = box.get_x() + box.get_width();
    const double start_bottom = box.get_y() + box.get_height();

    // spdlog::info("start pos:\nstart_left: {}\nstart_top: {}\nstart_right: {}\nstart_bottom:
    // {}",start_left, start_top, start_right, start_bottom);

    // Find next pos
    const double end_left = start_left + vel_x;
    const double end_top = start_top + vel_y;
    const double end_right = start_right + vel_x;
    const double end_bottom = start_bottom + vel_y;

    // spdlog::info("end pos:\nend_left: {}\nend_top: {}\nend_right: {}\nend_bottom: {}",end_left,
    // end_top, end_right, end_bottom);

    // Calculate bounding box
    double bb_left = std::floor(std::fmin(start_left, end_left) / tile);
    double bb_top = std::floor(std::fmin(start_top, end_top) / tile);
    double bb_right = std::floor(std::fmax(start_right, end_right) / tile);
    double bb_bottom = std::floor(std::fmax(start_bottom, end_bottom) / tile);

    // spdlog::info("bb pos:\nbb_left: {}\nbb_top: {}\nbb_right: {}\nbb_bottom: {}",bb_left, bb_top,
    // bb_right, bb_bottom);

    // Clamp bounding box to valid tile indices [0, grid-1]. Clamping is done in
    // the floating-point domain: converting a negative double to size_t first
    // would wrap to a huge value and defeat the lower bound.
    const double max_x = static_cast<double>(grid_x - 1);
    const double max_y = static_cast<double>(grid_y - 1);
    std::size_t tile_left = static_cast<std::size_t>(std::clamp(bb_left, 0.0, max_x));
    std::size_t tile_top = static_cast<std::size_t>(std::clamp(bb_top, 0.0, max_y));
    std::size_t tile_right = static_cast<std::size_t>(std::clamp(bb_right, 0.0, max_x));
    std::size_t tile_bottom = static_cast<std::size_t>(std::clamp(bb_bottom, 0.0, max_y));

    // spdlog::info("tile pos:\ntile_left: {}\ntile_top: {}\ntile_right: {}\ntile_bottom:
    // {}",tile_left, tile_top, tile_right, tile_bottom);

    std::set<std::size_t> res;
    // Push all tiles inside bounding box to colliders buffer
    for (std::size_t y = tile_top; y <= tile_bottom; y++) {
        for (std::size_t x = tile_left; x <= tile_right; x++) {
            std::size_t index = y * grid_x + x;
            res.insert(index);
            // spdlog::info("+");
        }
    }

    return res;
}
}  // namespace lit::game
