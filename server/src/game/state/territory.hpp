#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "state/map_scale.hpp"

namespace lit::game {
// Territory grid: every cell's owner plus in-progress capture state. Changed
// cells are marked dirty and go out as CellUpdate in the next snapshot.
struct Territory {
    std::uint32_t width{0};  // in cells
    std::uint32_t height{0};
    std::vector<std::uint8_t> owners;           // faction id, 0 = neutral
    std::vector<std::uint8_t> capture_faction;  // who is capturing (0 = none)
    std::vector<double> capture_progress;       // 0..100
    std::vector<bool> protected_cells;          // in a capital's zone: its enemies can't take it
    std::unordered_set<std::uint32_t> active;   // cells with progress > 0
    std::unordered_set<std::uint32_t> dirty;    // changed since the last snapshot

    void reset(std::uint32_t w, std::uint32_t h) {
        width = w;
        height = h;
        const std::size_t n = static_cast<std::size_t>(w) * h;
        owners.assign(n, 0);
        capture_faction.assign(n, 0);
        capture_progress.assign(n, 0.0);
        protected_cells.assign(n, false);
        active.clear();
        dirty.clear();
    }

    // Index of the cell under a world point (clamped onto the map).
    std::uint32_t index_at(double x, double y) const {
        const auto col = std::min(static_cast<std::uint32_t>(x / kUnitsPerCell), width - 1);
        const auto row = std::min(static_cast<std::uint32_t>(y / kUnitsPerCell), height - 1);
        return row * width + col;
    }
};
}  // namespace lit::game
