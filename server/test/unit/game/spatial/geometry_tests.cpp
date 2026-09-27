#include <gtest/gtest.h>

#include "spatial/geometry.hpp"

// A point moving (0,0) -> (100,0) unless stated otherwise; t is the fraction of
// the segment at first contact.
using lit::game::segment_circle_hit;

TEST(SegmentCircle, HitsCircleOnThePathAtFirstContact) {
    const auto t = segment_circle_hit(0, 0, 100, 0, 50, 0, 10);
    ASSERT_TRUE(t.has_value());
    EXPECT_NEAR(*t, 0.4, 1e-9);  // touches the circle's near edge at x = 40
}

TEST(SegmentCircle, MissesWhenPassingBesideTheCircle) {
    EXPECT_FALSE(segment_circle_hit(0, 0, 100, 0, 50, 20, 10).has_value());
}

TEST(SegmentCircle, GrazingCountsAsHit) {
    const auto t = segment_circle_hit(0, 0, 100, 0, 50, 10, 10);
    ASSERT_TRUE(t.has_value());
    EXPECT_NEAR(*t, 0.5, 1e-9);
}

TEST(SegmentCircle, StartingInsideHitsAtOnce) {
    const auto t = segment_circle_hit(0, 0, 100, 0, 0, 0, 10);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(*t, 0.0);
}

TEST(SegmentCircle, CircleBeyondTheEndIsMissed) {
    EXPECT_FALSE(segment_circle_hit(0, 0, 100, 0, 150, 0, 10).has_value());
}

TEST(SegmentCircle, ContactExactlyAtTheEndCounts) {
    const auto t = segment_circle_hit(0, 0, 100, 0, 110, 0, 10);
    ASSERT_TRUE(t.has_value());
    EXPECT_NEAR(*t, 1.0, 1e-9);
}

TEST(SegmentCircle, CircleBehindTheStartIsMissed) {
    EXPECT_FALSE(segment_circle_hit(0, 0, 100, 0, -50, 0, 10).has_value());
}

TEST(SegmentCircle, ZeroLengthSegment) {
    const auto inside = segment_circle_hit(0, 0, 0, 0, 5, 0, 10);
    ASSERT_TRUE(inside.has_value());
    EXPECT_EQ(*inside, 0.0);
    EXPECT_FALSE(segment_circle_hit(0, 0, 0, 0, 50, 0, 10).has_value());
}
