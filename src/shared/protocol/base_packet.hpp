#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <cstring>
#include <type_traits>

namespace ep::net
{
  namespace packet_info
  {
    inline constexpr std::uint32_t kMaxPayloadSize = 128;
  }

  template<typename T>
  concept PodType = std::is_standard_layout_v<T>;

#pragma pack(push, 1)
  struct PacketHead {
    std::uint16_t opcode_;
    std::uint32_t size_;
    
    inline bool IsValid() const noexcept;
  };
#pragma pack(pop)

  bool PacketHead::IsValid() const noexcept
  {
    return (opcode_ <= 0xFF && size_ < packet_info::kMaxPayloadSize);
  }

  struct NetPacket {
    PacketHead head_{};
    std::vector<std::uint8_t> body_;

    // TODO not convert to htonl/htons
    // Serialize data
    template<PodType T>
    friend NetPacket& operator<<(NetPacket& packet, T value);

    // TODO not convert to ntohl/ntohs
    // Deserialize data
    template<PodType T>
    friend NetPacket& operator>>(NetPacket& packet, T& value);
  };

  template<PodType T>
  NetPacket& operator<<(NetPacket& packet, T value)
  {
    // Allocate memmory and copy value into buffer
    std::size_t i = packet.body_.size();
    packet.body_.resize(i + sizeof(T));
    std::memcpy(packet.body_.data() + i, &value, sizeof(T));

    // Update size
    packet.head_.size_= packet.body_.size();

    return packet;
  }

  template<PodType T>
  NetPacket& operator>>(NetPacket& packet, T& value)
  {
    // Copy payload data into value and resize buffer
    std::size_t i = packet.body_.size() - sizeof(T);
    memcpy(&value, packet.body_.data() + i, sizeof(T));
    packet.body_.resize(i);

    // Update size
    packet.head_.size_ = packet.body_.size();

    return packet;
  }

  struct GamePacket {
    NetPacket packet_;
    std::size_t id_;

    explicit GamePacket(NetPacket packet, std::size_t id) :
      packet_(std::move(packet)),
      id_(id)
   {}

    uint16_t GetOpcode() const noexcept { return packet_.head_.opcode_; }
  };
}
