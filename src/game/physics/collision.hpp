#pragma once

#include <vector>
#include <set>

#include "tile/i_box.hpp"
#include "tile/tile.hpp"
#include "swept_math.hpp"

namespace ep::game
{
  class Collision {
  public:
    // Calculate sweptAABB collsion along one axis
    SweptData SweptAxis(const IBox &box, 
                        std::uint8_t tile, std::uint16_t grid_x, std::uint8_t grid_y,
                        const std::vector<Tile> &map, 
                        double vel_x, double vel_y);
  private:
    // Find potential collision tiles
    std::set<std::size_t> FindCollisionIndices(const IBox& box, 
                                               uint8_t tile, std::uint16_t grid_x, std::uint8_t grid_y, 
                                               double vel_x, double vel_y);
  };
}
