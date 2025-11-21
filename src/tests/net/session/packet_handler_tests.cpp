#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include "protocol/base_packet.hpp"
#include "session/packet_handler.hpp"

TEST(PacketHandlerTest, Init)
{
  ep::net::PacketHandler handler;

  EXPECT_EQ(handler.HeadSizeLeft(), sizeof(ep::net::PacketHead));
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
  
  std::uint16_t opcode = ep::net::swap_endian(5);
  std::uint32_t size = ep::net::swap_endian(6);
  
  memcpy(handler.HeadCurrentData(), &opcode, sizeof(opcode));
  EXPECT_EQ(handler.UpdateHeadSize(sizeof(opcode)), false);
    
  memcpy(handler.HeadCurrentData(), &size, sizeof(size));
  EXPECT_EQ(handler.UpdateHeadSize(sizeof(size)), true);
}

TEST(PacketHandlerTest, UpdateBodySize)
{
  ep::net::PacketHandler handler;
  ep::net::PacketHead head;
  head.opcode_ = ep::net::swap_endian(1);
  head.size_ = ep::net::swap_endian(3);

  memcpy(handler.HeadCurrentData(), &head, sizeof(ep::net::PacketHead));
  handler.UpdateHeadSize(sizeof(ep::net::PacketHead));

  EXPECT_EQ(handler.BodySizeLeft(), 3);
  EXPECT_EQ(handler.UpdateBodySize(2), false);
  EXPECT_EQ(handler.UpdateBodySize(1), true);
}
