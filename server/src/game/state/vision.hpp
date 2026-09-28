#pragma once

#include <cstdint>
#include <vector>

namespace lit::game {
// What one faction sees right now (fog of war): a flag per map cell, row-major
// like Territory. Default-constructed it sees nothing.
struct Vision {
    std::vector<std::uint8_t> cells;  // 1 = visible

    bool sees(std::uint32_t index) const { return index < cells.size() && cells[index] != 0; }
};
}  // namespace lit::game
