#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "protocol/net_packet.hpp"
#include "session/packet_handler.hpp"

TEST(PacketHandlerTest, Init) {
    ep::net::PacketHandler handler;

    EXPECT_EQ(handler.head_size_left(), sizeof(ep::PacketHead));
    EXPECT_EQ(handler.body_size_left(), 0);
}

TEST(PacketHandlerTest, GetHeadData) {
    ep::net::PacketHandler handler;

    EXPECT_NE(handler.head_current_data(), nullptr);
}

TEST(PacketHandlerTest, GetBodyData) {
    ep::net::PacketHandler handler;

    EXPECT_EQ(handler.body_current_data(), nullptr);
}

TEST(PacketHandlerTest, UpdateHeadSize) {
    ep::net::PacketHandler handler;

    std::uint16_t opcode = ep::swap_endian(std::uint16_t{5});
    std::uint32_t size = ep::swap_endian(std::uint32_t{6});

    memcpy(handler.head_current_data(), &opcode, sizeof(opcode));
    EXPECT_EQ(handler.update_head_size(sizeof(opcode)), false);

    memcpy(handler.head_current_data(), &size, sizeof(size));
    EXPECT_EQ(handler.update_head_size(sizeof(size)), true);
}

TEST(PacketHandlerTest, UpdateBodySize) {
    ep::net::PacketHandler handler;
    ep::PacketHead head;
    head.opcode = ep::swap_endian(std::uint16_t{1});
    head.size = ep::swap_endian(std::uint32_t{3});

    memcpy(handler.head_current_data(), &head, sizeof(ep::PacketHead));
    handler.update_head_size(sizeof(ep::PacketHead));

    EXPECT_EQ(handler.body_size_left(), 3);
    // EXPECT_EQ(handler.UpdateBodySize(2), false);
    // EXPECT_EQ(handler.UpdateBodySize(1), true);
}
