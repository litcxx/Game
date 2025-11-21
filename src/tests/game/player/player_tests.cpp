#include <gtest/gtest.h>

#include "player/player.hpp"

TEST(Player, InitTest)
{
  ep::game::Player p(1, 2, 3);

  EXPECT_EQ(p.GetX(), 1);
  EXPECT_EQ(p.GetY(), 2);
  EXPECT_EQ(p.GetID(), 3);
}

TEST(Player, Move)
{
  ep::game::Player p(0, 0, 0);
  
  double dx = 1.5;
  double dy = 3.14;
  p.Move(dx, dy);
  
  EXPECT_EQ(p.GetX(), dx);
  EXPECT_EQ(p.GetY(), dy);
}
