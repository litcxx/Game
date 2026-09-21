#include <gtest/gtest.h>

#include "protocol/net_packet.hpp"

TEST(NetPacketTest, HeaderGetters) {
    lit::NetPacket packet;
    EXPECT_EQ(packet.get_head_opcode(), 0);
    EXPECT_EQ(packet.get_head_size(), 0);
    EXPECT_NE(packet.get_head_data(), nullptr);
}

TEST(NetPacketTest, BodyGetters) {
    lit::NetPacket packet;
    EXPECT_EQ(packet.get_body_size(), 0);
    EXPECT_EQ(packet.get_body_data(), nullptr);
}

TEST(NetPacketTest, OperatorInOut) {
    lit::NetPacket packet;

    packet << 3.14 << 0.5f << 521;
    EXPECT_EQ(packet.get_body_size(), sizeof(double) + sizeof(float) + sizeof(int));
    EXPECT_EQ(packet.get_head_size(), sizeof(double) + sizeof(float) + sizeof(int));
    int i;
    float f;
    double d;
    packet >> i >> f >> d;
    EXPECT_EQ(packet.get_head_size(), 0);
    EXPECT_EQ(packet.get_body_size(), 0);

    EXPECT_EQ(i, 521);
    EXPECT_EQ(f, 0.5f);
    EXPECT_EQ(d, 3.14);
}
