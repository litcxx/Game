#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <cstring>
#include <type_traits>
#include <algorithm>

namespace ep::net
{
  template<typename T>
  static T swap_endian(T value)
  {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(&value);
    std::reverse(ptr, ptr + sizeof(T));
    return *reinterpret_cast<T*>(ptr);
  }
  
  namespace packet_info
  {
    inline constexpr std::uint16_t kMaxOpcode = 0x0FFF;
    inline constexpr std::uint32_t kMaxPayloadSize = 128;
  }

  template<typename T>
  concept PodType = std::is_standard_layout_v<T>;

#pragma pack(push, 1)
  struct PacketHead {
    std::uint16_t opcode_;
    std::uint32_t size_;
    
  };
#pragma pack(pop)

  struct NetPacket {
    PacketHead head_{};
    std::vector<std::uint8_t> body_;

    uint16_t GetOpcode() const noexcept { return swap_endian(head_.opcode_); }
    uint32_t GetSize() const noexcept { return swap_endian(head_.size_); }
    bool IsValidHeader() const noexcept;
    void Resize() { body_.resize(GetSize()); }

    // Serialize data
    template<PodType T>
    friend NetPacket& operator<<(NetPacket& packet, T value);

    // Deserialize data
    template<PodType T>
    friend NetPacket& operator>>(NetPacket& packet, T& value);
  };

  inline bool NetPacket::IsValidHeader() const noexcept
  { 
    return swap_endian(head_.opcode_) < packet_info::kMaxOpcode && 
           swap_endian(head_.size_) < packet_info::kMaxPayloadSize;
  }

  template<PodType T>
  NetPacket& operator<<(NetPacket& packet, T value)
  {
    // Convert from host byte order to network byte order.
    value = swap_endian(value);

    // Allocate memmory and copy value into buffer
    std::size_t offset = packet.body_.size();
    packet.body_.resize(offset + sizeof(T));
    std::memcpy(packet.body_.data() + offset, &value, sizeof(T));

    // Update size
    packet.head_.size_= packet.body_.size();

    return packet;
  }

  template<PodType T>
  NetPacket& operator>>(NetPacket& packet, T& value)
  {
    // Copy payload data into value and resize buffer.
    std::size_t offset = packet.body_.size() - sizeof(T);
    memcpy(&value, packet.body_.data() + offset, sizeof(T));
  
    // Convert from netwrok byte order to host byte order.
    value = swap_endian(value);

    packet.body_.resize(offset);

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
    std::size_t GetID() const noexcept { return id_; }
  };
}
