#include "systems/capital_system.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace lit::game {
std::vector<std::uint32_t> capital_zone(const CapitalConfig& capital, std::uint32_t width,
                                        std::uint32_t height) {
    const std::int64_t col = capital.cell % width;
    const std::int64_t row = capital.cell / width;
    const std::int64_t r = capital.protected_radius;
    std::vector<std::uint32_t> zone;
    for (std::int64_t y = std::max<std::int64_t>(row - r, 0);
         y <= std::min<std::int64_t>(row + r, std::int64_t{height} - 1); ++y) {
        for (std::int64_t x = std::max<std::int64_t>(col - r, 0);
             x <= std::min<std::int64_t>(col + r, std::int64_t{width} - 1); ++x) {
            if ((x - col) * (x - col) + (y - row) * (y - row) <= r * r) {
                zone.push_back(static_cast<std::uint32_t>(y * width + x));
            }
        }
    }
    return zone;
}

void seed_capitals(Territory& territory, const std::vector<CapitalConfig>& capitals) {
    for (const CapitalConfig& capital : capitals) {
        for (std::uint32_t index : capital_zone(capital, territory.width, territory.height)) {
            territory.set_owner(index, static_cast<std::uint8_t>(capital.faction_id));
        }
    }
}

void protect_capitals(Territory& territory, const std::vector<CapitalConfig>& capitals) {
    for (const CapitalConfig& capital : capitals) {
        for (std::uint32_t index : capital_zone(capital, territory.width, territory.height)) {
            territory.protected_cells[index] = true;
        }
    }
}
}  // namespace lit::game
