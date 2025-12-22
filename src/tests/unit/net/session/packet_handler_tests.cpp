#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include "protocol/net_packet.hpp"
#include "session/packet_handler.hpp"

TEST(PacketHandlerTest, Init)
{
  ep::net::PacketHandler handler;

  EXPECT_EQ(handler.HeadSizeLeft(), sizeof(ep::PacketHead));
  EXPECT_EQ(handler.BodySizeLeft(), 0);
}

TEST(PacketHandlerTest, GetHeadData)
{
  ep::net::PacketHandler handler;

  EXPECT_NE(handler.HeadCurrentData(), nullptr);
}

TEST(PacketHandlerTest, GetBodyData)
{
  ep::net::PacketHandler handler;

  EXPECT_EQ(handler.BodyCurrentData(), nullptr);
}

TEST(PacketHandlerTest, UpdateHeadSize)
{
  ep::net::PacketHandler handler;
  
  std::uint16_t opcode = ep::swap_endian(5);
  std::uint32_t size = ep::swap_endian(6);
  
  memcpy(handler.HeadCurrentData(), &opcode, sizeof(opcode));
  EXPECT_EQ(handler.UpdateHeadSize(sizeof(opcode)), false);
    
  memcpy(handler.HeadCurrentData(), &size, sizeof(size));
  EXPECT_EQ(handler.UpdateHeadSize(sizeof(size)), true);
}

TEST(PacketHandlerTest, UpdateBodySize)
{
  ep::net::PacketHandler handler;
  ep::PacketHead head;
  head.opcode_ = ep::swap_endian(1);
  head.size_ = ep::swap_endian(3);

  memcpy(handler.HeadCurrentData(), &head, sizeof(ep::PacketHead));
  handler.UpdateHeadSize(sizeof(ep::PacketHead));

  EXPECT_EQ(handler.BodySizeLeft(), 3);
  // EXPECT_EQ(handler.UpdateBodySize(2), false);
  // EXPECT_EQ(handler.UpdateBodySize(1), true);
}
