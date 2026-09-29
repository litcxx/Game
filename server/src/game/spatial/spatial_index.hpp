#pragma once

#include <cstdint>
#include <vector>

namespace lit::game {
// Uniform-grid broad phase over points (unit centres, keyed by unit id). Rebuilt
// every tick (clear + insert). A radius query scans only the buckets overlapping
// the circle's bounding box, then filters by exact distance.
class SpatialIndex {
  public:
    // width/height: world extent in units; bucket_size: bucket edge in units.
    SpatialIndex(double width, double height, double bucket_size);

    // Remove every entry: O(occupied buckets), bucket capacity is kept.
    void clear();
    void insert(std::uint32_t key, double x, double y);

    // Call fn(key) for every entry with (x - cx)^2 + (y - cy)^2 <= radius^2
    // (inclusive). Visiting order is unspecified; fn must not modify the index.
    template <typename Fn>
    void for_each_in_radius(double cx, double cy, double radius, Fn&& fn) const {
        const std::uint32_t col0 = col_of(cx - radius);
        const std::uint32_t col1 = col_of(cx + radius);
        const std::uint32_t row0 = row_of(cy - radius);
        const std::uint32_t row1 = row_of(cy + radius);
        const double radius_sq = radius * radius;
        for (std::uint32_t row = row0; row <= row1; ++row) {
            for (std::uint32_t col = col0; col <= col1; ++col) {
                for (const Entry& e : buckets_[row * cols_ + col]) {
                    const double dx = e.x - cx;
                    const double dy = e.y - cy;
                    if (dx * dx + dy * dy <= radius_sq) fn(e.key);
                }
            }
        }
    }

  private:
    struct Entry {
        std::uint32_t key;
        double x;
        double y;
    };

    // Bucket column / row of a coordinate, clamped onto the grid.
    std::uint32_t col_of(double x) const;
    std::uint32_t row_of(double y) const;

    double bucket_size_;
    std::uint32_t cols_;
    std::uint32_t rows_;
    std::vector<std::vector<Entry>> buckets_;  // row-major, cols_ * rows_
    std::vector<std::uint32_t> occupied_;      // buckets holding entries (for clear)
};
}  // namespace lit::game
