#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "protocol/net_packet.hpp"
#include "session/packet_handler.hpp"

TEST(PacketHandlerTest, Init) {
    lit::net::PacketHandler handler;

    EXPECT_EQ(handler.head_size_left(), sizeof(lit::PacketHead));
    EXPECT_EQ(handler.body_size_left(), 0);
}

TEST(PacketHandlerTest, GetHeadData) {
    lit::net::PacketHandler handler;

    EXPECT_NE(handler.head_current_data(), nullptr);
}

TEST(PacketHandlerTest, GetBodyData) {
    lit::net::PacketHandler handler;

    EXPECT_EQ(handler.body_current_data(), nullptr);
}

TEST(PacketHandlerTest, UpdateHeadSize) {
    lit::net::PacketHandler handler;

    std::uint16_t opcode = lit::swap_endian(std::uint16_t{5});
    std::uint32_t size = lit::swap_endian(std::uint32_t{6});

    memcpy(handler.head_current_data(), &opcode, sizeof(opcode));
    EXPECT_EQ(handler.update_head_size(sizeof(opcode)), false);

    memcpy(handler.head_current_data(), &size, sizeof(size));
    EXPECT_EQ(handler.update_head_size(sizeof(size)), true);
}

TEST(PacketHandlerTest, UpdateBodySize) {
    lit::net::PacketHandler handler;
    lit::PacketHead head;
    head.opcode = lit::swap_endian(std::uint16_t{1});
    head.size = lit::swap_endian(std::uint32_t{3});

    memcpy(handler.head_current_data(), &head, sizeof(lit::PacketHead));
    handler.update_head_size(sizeof(lit::PacketHead));

    EXPECT_EQ(handler.body_size_left(), 3);
    // EXPECT_EQ(handler.UpdateBodySize(2), false);
    // EXPECT_EQ(handler.UpdateBodySize(1), true);
}
