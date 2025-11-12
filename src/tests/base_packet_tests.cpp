#include <gtest/gtest.h>

#include "protocol/base_packet.hpp"

TEST(BasePacketTest, Foo)
{
  ep::net::PacketData packet;

  packet << 3.14 << 0.5f << 521;
  int i;
  float f;
  double d;
  packet >> i >> f >> d;

  EXPECT_EQ(i, 521);
  EXPECT_EQ(f, 0.5f);
  EXPECT_EQ(d, 3.14);
}
