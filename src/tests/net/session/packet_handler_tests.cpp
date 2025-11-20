#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include "protocol/base_packet.hpp"
#include "session/packet_handler.hpp"

TEST(PacketHandlerTest, Init)
{
  ep::net::PacketHandler handler;

  EXPECT_EQ(handler.GetHeadRead(), 0);
  EXPECT_EQ(handler.GetBodyRead(), 0);
}

TEST(PacketHandlerTest, GetHeaderData)
{
  ep::net::PacketHandler handler;

  EXPECT_NE(handler.HeaderData(), nullptr);
}

TEST(PacketHandlerTest, GetBodyData)
{
  ep::net::PacketHandler handler;

  EXPECT_EQ(handler.BodyData(), nullptr);
}

TEST(PacketHandlerTest, PacketRemainingSize)
{
  ep::net::PacketHandler handler;

  EXPECT_GT(handler.HeaderSize(), 0);
  EXPECT_EQ(handler.BodySize(), 0);
}

TEST(PacketHandlerTest, ReadHalfHeader)
{
  ep::net::PacketHandler handler;
  
  EXPECT_EQ(handler.ReadHeader(handler.HeaderSize() / 2), false);
}

TEST(PacketHandlerTest, ReadFullHeader)
{
  ep::net::PacketHandler handler;
  
  EXPECT_EQ(handler.ReadHeader(handler.HeaderSize()), true);
}

TEST(PacketHandlerTest, ReadHalfBody)
{
  ep::net::PacketHandler handler;

  uint16_t opcode = ep::net::swap_endian(5);
  uint32_t size = ep::net::swap_endian(6);
  
  memcpy(handler.HeaderData(), &opcode, sizeof(opcode));
  EXPECT_EQ(handler.ReadHeader(sizeof(opcode)), false);
    
  memcpy(handler.HeaderData(), &size, sizeof(size));
  EXPECT_EQ(handler.ReadHeader(sizeof(size)), true);

  EXPECT_EQ(handler.ReadBody(handler.BodySize() / 2), false);
}

TEST(PacketHandlerTest, ReadFullBody)
{
  ep::net::PacketHandler handler;

  uint16_t opcode = ep::net::swap_endian(5);
  uint32_t size = ep::net::swap_endian(6);
  
  memcpy(handler.HeaderData(), &opcode, sizeof(opcode));
  EXPECT_EQ(handler.ReadHeader(sizeof(opcode)), false);
    
  memcpy(handler.HeaderData(), &size, sizeof(size));
  EXPECT_EQ(handler.ReadHeader(sizeof(size)), true);

  EXPECT_EQ(handler.ReadBody(handler.BodySize()), true);
}
