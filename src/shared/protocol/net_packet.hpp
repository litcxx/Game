#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <vector>

#include "protocol/opcodes.hpp"

namespace ep {
template <typename T>
static T swap_endian(T value) {
    std::uint8_t* ptr = reinterpret_cast<std::uint8_t*>(&value);
    std::reverse(ptr, ptr + sizeof(T));
    return *reinterpret_cast<T*>(ptr);
}

namespace packet_info {
constexpr std::uint16_t kMaxOpcode = 0x0FFF;
constexpr std::uint32_t kMaxPayloadSize = 128;
}  // namespace packet_info

template <typename T>
concept PodType = std::is_standard_layout_v<T>;

#pragma pack(push, 1)
struct PacketHead {
    std::uint16_t opcode;
    std::uint32_t size;
};
#pragma pack(pop)

class NetPacket {
    // Serialize data.
    template <PodType T>
    friend NetPacket& operator<<(NetPacket& packet, T value);

    // Deserialize data.
    template <PodType T>
    friend NetPacket& operator>>(NetPacket& packet, T& value);

  public:
    NetPacket() = default;
    explicit NetPacket(Opcodes opcode);
    NetPacket(const NetPacket&) = delete;
    NetPacket& operator=(const NetPacket&) = delete;
    NetPacket(NetPacket&& other);
    NetPacket& operator=(NetPacket&& other);
    ~NetPacket() = default;

    // Get header data.
    std::uint16_t get_head_opcode() const noexcept { return swap_endian(head_.opcode); }
    std::uint32_t get_head_size() const noexcept { return swap_endian(head_.size); }
    std::uint8_t* get_head_data() noexcept { return reinterpret_cast<std::uint8_t*>(&head_); }

    // Set header data.
    void set_head_opcode(std::uint16_t opcode) noexcept { head_.opcode = swap_endian(opcode); }
    void set_head_size(std::uint32_t size) noexcept { head_.size = swap_endian(size); }

    // Get body data.
    std::size_t get_body_size() const noexcept;
    std::uint8_t* get_body_data() noexcept;

    bool is_valid_header() const noexcept;
    void resize_body(std::size_t size);
    std::vector<std::uint8_t> make_buffer();

  private:
    PacketHead head_{};
    std::unique_ptr<std::vector<std::uint8_t>> body_;
};

template <PodType T>
NetPacket& operator<<(NetPacket& packet, T value) {
    if (!packet.body_) packet.body_ = std::make_unique<std::vector<std::uint8_t>>();

    // Convert from host byte order to network byte order.
    value = swap_endian(value);

    // Allocate memmory and copy value into buffer
    std::size_t offset = packet.get_body_size();
    packet.resize_body(offset + sizeof(T));
    std::memcpy(packet.get_body_data() + offset, &value, sizeof(T));

    // Update size
    packet.set_head_size(static_cast<std::uint32_t>(packet.get_body_size()));

    return packet;
}

template <PodType T>
NetPacket& operator>>(NetPacket& packet, T& value) {
    // Copy payload data into value and resize buffer.
    std::size_t offset = packet.get_body_size() - sizeof(T);
    memcpy(&value, packet.get_body_data() + offset, sizeof(T));

    // Convert from netwrok byte order to host byte order.
    value = swap_endian(value);

    // Update size
    packet.resize_body(offset);
    packet.set_head_size(static_cast<std::uint32_t>(offset));

    return packet;
}

NetPacket create_player_packet(std::size_t id, double x, double y, std::uint8_t width,
                               std::uint8_t height);
NetPacket add_player_packet(std::size_t id, double x, double y, std::uint8_t width,
                            std::uint8_t height);
NetPacket remove_player_packet(std::size_t id);
NetPacket move_player_packet(std::size_t id, double x, double y);
}  // namespace ep
