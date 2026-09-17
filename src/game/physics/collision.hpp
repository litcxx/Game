#pragma once

#include <set>
#include <vector>

#include "swept_math.hpp"
#include "tile/i_box.hpp"
#include "tile/tile.hpp"

namespace ep::game {
class Collision {
  public:
    // Calculate sweptAABB collsion along one axis
    SweptData swept_axis(const IBox &box, std::uint8_t tile, std::uint16_t grid_x,
                         std::uint8_t grid_y, const std::vector<Tile> &map, double vel_x,
                         double vel_y);

  private:
    // Find potential collision tiles
    std::set<std::size_t> find_collision_indices(const IBox &box, uint8_t tile,
                                                 std::uint16_t grid_x, std::uint8_t grid_y,
                                                 double vel_x, double vel_y);
};
}  // namespace ep::game
