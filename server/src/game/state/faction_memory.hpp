#pragma once

#include <cstdint>
#include <vector>

#include "state/territory.hpp"
#include "state/vision.hpp"

namespace lit::game {
// What one faction remembers of the map (fog of war, GDD 7.4, F-3): the cells it
// has ever seen, and each one's owner as it last saw it. A faction has one sight,
// so it has one memory: every member gets it whole — on coming back, and a
// newcomer on joining the faction. Kept for the season, and saved.
struct FactionMemory {
    std::vector<std::uint8_t> explored;  // 1 = seen at least once, per cell (row-major)
    std::vector<std::uint8_t> owners;    // each cell's owner when last seen; 0 where never seen

    // One snapshot's sight: every cell visible now is explored, its owner as it is now.
    void see(const Territory& territory, const Vision& vision) {
        if (explored.size() != territory.owners.size()) {
            explored.assign(territory.owners.size(), 0);
            owners.assign(territory.owners.size(), 0);
        }
        for (std::uint32_t index = 0; index < vision.cells.size() && index < owners.size();
             ++index) {
            if (vision.cells[index] == 0) continue;
            explored[index] = 1;
            owners[index] = territory.owners[index];
        }
    }

    // Whether the faction has ever seen the cell.
    bool knows(std::uint32_t index) const {
        return index < explored.size() && explored[index] != 0;
    }
};
}  // namespace lit::game
