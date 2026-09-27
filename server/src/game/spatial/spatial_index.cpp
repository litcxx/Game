#include "spatial/spatial_index.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace lit::game {
namespace {
// Buckets needed to cover `extent` (at least one).
std::uint32_t buckets_along(double extent, double bucket_size) {
    return std::max(1U, static_cast<std::uint32_t>(std::ceil(extent / bucket_size)));
}

// Bucket coordinate of `v`, clamped to [0, count - 1] (no out-of-range cast).
std::uint32_t clamp_bucket(double v, double bucket_size, std::uint32_t count) {
    const double b = std::floor(v / bucket_size);
    if (!(b > 0.0)) return 0;  // negative (or NaN)
    return static_cast<std::uint32_t>(std::min(b, static_cast<double>(count - 1)));
}
}  // namespace

SpatialIndex::SpatialIndex(double width, double height, double bucket_size)
    : bucket_size_{bucket_size},
      cols_{buckets_along(width, bucket_size)},
      rows_{buckets_along(height, bucket_size)},
      buckets_(static_cast<std::size_t>(cols_) * rows_) {}

void SpatialIndex::clear() {
    for (std::uint32_t b : occupied_) buckets_[b].clear();
    occupied_.clear();
}

void SpatialIndex::insert(std::uint64_t key, double x, double y) {
    const std::uint32_t b = row_of(y) * cols_ + col_of(x);
    if (buckets_[b].empty()) occupied_.push_back(b);
    buckets_[b].push_back(Entry{key, x, y});
}

std::uint32_t SpatialIndex::col_of(double x) const { return clamp_bucket(x, bucket_size_, cols_); }

std::uint32_t SpatialIndex::row_of(double y) const { return clamp_bucket(y, bucket_size_, rows_); }
}  // namespace lit::game
