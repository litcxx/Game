#include <gtest/gtest.h>

#include "protocol/base_packet.hpp"

TEST(NetPacketTest, OperatorInOut)
{
  ep::NetPacket packet;
  EXPECT_EQ(packet.GetHeadSize(), 0);
  EXPECT_EQ(packet.GetBodySize(), 0);

  packet << 3.14 << 0.5f << 521;
  EXPECT_EQ(packet.GetBodySize(), sizeof(double) + sizeof(float) + sizeof(int));
  EXPECT_EQ(packet.GetHeadSize(), sizeof(double) + sizeof(float) + sizeof(int));
  int i;
  float f;
  double d;
  packet >> i >> f >> d;
  EXPECT_EQ(packet.GetHeadSize(), 0);
  EXPECT_EQ(packet.GetBodySize(), 0);

  EXPECT_EQ(i, 521);
  EXPECT_EQ(f, 0.5f);
  EXPECT_EQ(d, 3.14);
}
