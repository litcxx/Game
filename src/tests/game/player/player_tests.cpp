#include <gtest/gtest.h>

#include "player/player.hpp"

TEST(Player, InitTest)
{
  ep::game::Player p(1, 2, 3, 4);

  EXPECT_EQ(p.GetX(), 1);
  EXPECT_EQ(p.GetY(), 2);
  EXPECT_EQ(p.GetZ(), 3);
  EXPECT_EQ(p.GetID(), 4);
}

TEST(Player, Move)
{
  ep::game::Player p(0, 0, 0, 0);
  
  double dx = 1.5;
  double dy = 3.14;
  double dz = 5.312;
  p.Move(dx, dy, dz);
  
  EXPECT_EQ(p.GetX(), dx);
  EXPECT_EQ(p.GetY(), dy);
  EXPECT_EQ(p.GetZ(), dz);
}
