#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "physics/swept_math.hpp"
#include "game/tile/mock_box.hpp"

namespace ep::tests
{
  class BoxesData {
  public:
    BoxesData(int x1, int y1, int x2, int y2, int tile, double vel_x, double vel_y) :
      vel_x_(vel_x), vel_y_(vel_y)
    {
      EXPECT_CALL(box1_, GetX())
        .WillRepeatedly(testing::Return(x1));
      EXPECT_CALL(box1_, GetY())
        .WillRepeatedly(testing::Return(y1));
      EXPECT_CALL(box1_, GetWidth())
        .WillRepeatedly(testing::Return(tile));
      EXPECT_CALL(box1_, GetHeight())
        .WillRepeatedly(testing::Return(tile));

      EXPECT_CALL(box2_, GetX())
        .WillRepeatedly(testing::Return(x2));
      EXPECT_CALL(box2_, GetY())
        .WillRepeatedly(testing::Return(y2));
      EXPECT_CALL(box2_, GetWidth())
        .WillRepeatedly(testing::Return(tile));
      EXPECT_CALL(box2_, GetHeight())
        .WillRepeatedly(testing::Return(tile));
    }
    double vel_x_, vel_y_;
    MockBox box1_, box2_;
  };

  TEST(SweptMathTests, SweptAABBNoCollisionVelXRight)
  {
    BoxesData bd(0, 0, 0, 32, 32, 1.0, 0.0);

    game::SweptData res = game::SweptAABB(bd.box1_, bd.box2_, bd.vel_x_, bd.vel_y_);

    EXPECT_EQ(res.hit_, false);
  }

  TEST(SweptMathTests, SweptAABBNoCollisionVelXLeft)
  {
    BoxesData bd(0, 0, 0, 32, 32, -1.0, 0.0);

    game::SweptData res = game::SweptAABB(bd.box1_, bd.box2_, bd.vel_x_, bd.vel_y_);

    EXPECT_EQ(res.hit_, false);
  }

  TEST(SweptMathTests, SweptAABBNoCollisionVelYTop)
  {
    BoxesData bd(0, 0, 32, 0, 32, 0.0, -1.0);

    game::SweptData res = game::SweptAABB(bd.box1_, bd.box2_, bd.vel_x_, bd.vel_y_);

    EXPECT_EQ(res.hit_, false);
  }

  TEST(SweptMathTests, SweptAABBNoCollisionVelYBottom)
  {
    BoxesData bd(0, 0, 32, 0, 32, 0.0, 1.0);

    game::SweptData res = game::SweptAABB(bd.box1_, bd.box2_, bd.vel_x_, bd.vel_y_);

    EXPECT_EQ(res.hit_, false);
  }

  TEST(SweptMathTests, SweptAABBCollisionVelXRight)
  {
    BoxesData bd(0, 0, 32, 0, 32, 1.0, 0.0);

    game::SweptData res = game::SweptAABB(bd.box1_, bd.box2_, bd.vel_x_, bd.vel_y_);

    EXPECT_EQ(res.hit_, true);
  }

  TEST(SweptMathTests, SweptAABBCollisionVelXLeft)
  {
    BoxesData bd(32, 0, 0, 0, 32, -1.0, 0.0);

    game::SweptData res = game::SweptAABB(bd.box1_, bd.box2_, bd.vel_x_, bd.vel_y_);

    EXPECT_EQ(res.hit_, true);
  }

  TEST(SweptMathTests, SweptAABBCollisionVelYTop)
  {
    BoxesData bd(0, 32, 0, 0, 32, 0.0, -1.0);

    game::SweptData res = game::SweptAABB(bd.box1_, bd.box2_, bd.vel_x_, bd.vel_y_);

    EXPECT_EQ(res.hit_, true);
  }

  TEST(SweptMathTests, SweptAABBCollisionVelYBottom)
  {
    BoxesData bd(0, 0, 0, 32, 32, 0.0, 1.0);

    game::SweptData res = game::SweptAABB(bd.box1_, bd.box2_, bd.vel_x_, bd.vel_y_);

    EXPECT_EQ(res.hit_, true);
  }

}
