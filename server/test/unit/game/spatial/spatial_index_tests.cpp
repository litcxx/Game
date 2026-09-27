#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "spatial/spatial_index.hpp"

namespace {

using Keys = std::vector<std::uint64_t>;

// Keys reported within `r` of (x, y), sorted (the visiting order is unspecified).
Keys query(const lit::game::SpatialIndex& index, double x, double y, double r) {
    Keys keys;
    index.for_each_in_radius(x, y, r, [&](std::uint64_t key) { keys.push_back(key); });
    std::ranges::sort(keys);
    return keys;
}

// Every test: a 1000x1000 world in 200-unit buckets (5x5).
lit::game::SpatialIndex make_index() { return lit::game::SpatialIndex(1000.0, 1000.0, 200.0); }

}  // namespace

TEST(SpatialIndex, ReportsEveryEntryWithinRadiusAndNothingElse) {
    auto index = make_index();
    index.insert(1, 110.0, 100.0);
    index.insert(2, 100.0, 150.0);
    index.insert(3, 700.0, 700.0);  // far away

    EXPECT_EQ(query(index, 100.0, 100.0, 60.0), (Keys{1, 2}));
}

TEST(SpatialIndex, RadiusIsInclusive) {
    auto index = make_index();
    index.insert(1, 220.0, 100.0);  // exactly 120 from the centre

    EXPECT_EQ(query(index, 100.0, 100.0, 120.0), (Keys{1}));
}

TEST(SpatialIndex, ExcludesEntryInScannedBucketButOutsideCircle) {
    auto index = make_index();
    index.insert(1, 200.0, 200.0);  // inside the query's bounding box, ~141 from the centre

    EXPECT_EQ(query(index, 100.0, 100.0, 120.0), Keys{});
}

TEST(SpatialIndex, FindsEntriesInAllBucketsAroundACorner) {
    auto index = make_index();
    // One entry in each of the four buckets that meet at (200, 200).
    index.insert(1, 190.0, 195.0);
    index.insert(2, 210.0, 195.0);
    index.insert(3, 195.0, 210.0);
    index.insert(4, 205.0, 205.0);

    EXPECT_EQ(query(index, 200.0, 200.0, 15.0), (Keys{1, 2, 3, 4}));
}

TEST(SpatialIndex, QueriesReachingPastTheMapEdgesAreClamped) {
    auto index = make_index();
    index.insert(1, 5.0, 5.0);
    index.insert(2, 1000.0, 1000.0);  // exactly on the far edge

    EXPECT_EQ(query(index, 0.0, 0.0, 300.0), (Keys{1}));
    EXPECT_EQ(query(index, 1000.0, 1000.0, 300.0), (Keys{2}));
}

TEST(SpatialIndex, ClearRemovesAllEntries) {
    auto index = make_index();
    index.insert(1, 100.0, 100.0);
    index.insert(2, 900.0, 900.0);

    index.clear();
    index.insert(3, 110.0, 100.0);

    EXPECT_EQ(query(index, 100.0, 100.0, 50.0), (Keys{3}));
    EXPECT_EQ(query(index, 900.0, 900.0, 50.0), Keys{});
}
