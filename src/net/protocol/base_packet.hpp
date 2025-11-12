#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <cstring>
#include <type_traits>
#include <iostream>

namespace ep::net
{
  struct PacketHead {
    std::uint16_t opcode_;
    std::uint32_t size_;
  };

  struct PacketData {
    PacketHead head_{};
    std::vector<std::uint8_t> body_;

    // Serialize data
    template<typename T>
    friend PacketData& operator<<(PacketData& packet, T value);

    // Deserialize data
    template<typename T>
    friend PacketData& operator>>(PacketData& packet, T& value);
  };

  template<typename T>
  PacketData& operator<<(PacketData& packet, T value)
  {
    // Check if T is standard layout type
    static_assert(std::is_standard_layout_v<T>);

    // Allocate memmory and copy value into buffer
    std::size_t i = packet.body_.size();
    packet.body_.resize(i + sizeof(T));
    std::memcpy(packet.body_.data() + i, &value, sizeof(T));

    // Update size
    packet.head_.size_= packet.body_.size();

    return packet;
  }

  template<typename T>
  PacketData& operator>>(PacketData& packet, T& value)
  {
    // Check if T is standard layout type
    static_assert(std::is_standard_layout_v<T>);

    // Copy payload data into value and resize buffer
    std::size_t i = packet.body_.size() - sizeof(T);
    memcpy(&value, packet.body_.data() + i, sizeof(T));
    packet.body_.resize(i);

    // Update size
    packet.head_.size_ = packet.body_.size();

    return packet;
  }

}
