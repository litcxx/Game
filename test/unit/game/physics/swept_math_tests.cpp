#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "game/tile/mock_box.hpp"
#include "physics/swept_math.hpp"

namespace ep::tests {
class BoxesData {
  public:
    BoxesData(int x1, int y1, int x2, int y2, int tile, double vel_x, double vel_y)
        : vel_x(vel_x), vel_y(vel_y) {
        EXPECT_CALL(box1, get_x()).WillRepeatedly(testing::Return(x1));
        EXPECT_CALL(box1, get_y()).WillRepeatedly(testing::Return(y1));
        EXPECT_CALL(box1, get_width()).WillRepeatedly(testing::Return(tile));
        EXPECT_CALL(box1, get_height()).WillRepeatedly(testing::Return(tile));

        EXPECT_CALL(box2, get_x()).WillRepeatedly(testing::Return(x2));
        EXPECT_CALL(box2, get_y()).WillRepeatedly(testing::Return(y2));
        EXPECT_CALL(box2, get_width()).WillRepeatedly(testing::Return(tile));
        EXPECT_CALL(box2, get_height()).WillRepeatedly(testing::Return(tile));
    }
    double vel_x, vel_y;
    MockBox box1, box2;
};

TEST(SweptMathTests, SweptAABBNoCollisionVelXRight) {
    BoxesData bd(0, 0, 0, 32, 32, 1.0, 0.0);

    game::SweptData res = game::swept_aabb(bd.box1, bd.box2, bd.vel_x, bd.vel_y);

    EXPECT_EQ(res.hit, false);
}

TEST(SweptMathTests, SweptAABBNoCollisionVelXLeft) {
    BoxesData bd(0, 0, 0, 32, 32, -1.0, 0.0);

    game::SweptData res = game::swept_aabb(bd.box1, bd.box2, bd.vel_x, bd.vel_y);

    EXPECT_EQ(res.hit, false);
}

TEST(SweptMathTests, SweptAABBNoCollisionVelYTop) {
    BoxesData bd(0, 0, 32, 0, 32, 0.0, -1.0);

    game::SweptData res = game::swept_aabb(bd.box1, bd.box2, bd.vel_x, bd.vel_y);

    EXPECT_EQ(res.hit, false);
}

TEST(SweptMathTests, SweptAABBNoCollisionVelYBottom) {
    BoxesData bd(0, 0, 32, 0, 32, 0.0, 1.0);

    game::SweptData res = game::swept_aabb(bd.box1, bd.box2, bd.vel_x, bd.vel_y);

    EXPECT_EQ(res.hit, false);
}

TEST(SweptMathTests, SweptAABBCollisionVelXRight) {
    BoxesData bd(0, 0, 32, 0, 32, 1.0, 0.0);

    game::SweptData res = game::swept_aabb(bd.box1, bd.box2, bd.vel_x, bd.vel_y);

    EXPECT_EQ(res.hit, true);
}

TEST(SweptMathTests, SweptAABBCollisionVelXLeft) {
    BoxesData bd(32, 0, 0, 0, 32, -1.0, 0.0);

    game::SweptData res = game::swept_aabb(bd.box1, bd.box2, bd.vel_x, bd.vel_y);

    EXPECT_EQ(res.hit, true);
}

TEST(SweptMathTests, SweptAABBCollisionVelYTop) {
    BoxesData bd(0, 32, 0, 0, 32, 0.0, -1.0);

    game::SweptData res = game::swept_aabb(bd.box1, bd.box2, bd.vel_x, bd.vel_y);

    EXPECT_EQ(res.hit, true);
}

TEST(SweptMathTests, SweptAABBCollisionVelYBottom) {
    BoxesData bd(0, 0, 0, 32, 32, 0.0, 1.0);

    game::SweptData res = game::swept_aabb(bd.box1, bd.box2, bd.vel_x, bd.vel_y);

    EXPECT_EQ(res.hit, true);
}

}  // namespace ep::tests
