#pragma once

#include "protocol/opcodes.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <cstring>
#include <type_traits>
#include <algorithm>

namespace ep
{
  template<typename T>
  static T swap_endian(T value)
  {
    std::uint8_t* ptr = reinterpret_cast<std::uint8_t*>(&value);
    std::reverse(ptr, ptr + sizeof(T));
    return *reinterpret_cast<T*>(ptr);
  }
  
  namespace packet_info
  {
    constexpr std::uint16_t kMaxOpcode = 0x0FFF;
    constexpr std::uint32_t kMaxPayloadSize = 128;
  }

  template<typename T>
  concept PodType = std::is_standard_layout_v<T>;

#pragma pack(push, 1)
  struct PacketHead {
    std::uint16_t opcode_;
    std::uint32_t size_;
  };
#pragma pack(pop)

  class NetPacket {
    // Serialize data.
    template<PodType T>
    friend NetPacket& operator<<(NetPacket& packet, T value);

    // Deserialize data.
    template<PodType T>
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
    std::uint16_t GetHeadOpcode() const noexcept { return swap_endian(head_.opcode_); }
    std::uint32_t GetHeadSize() const noexcept { return swap_endian(head_.size_); }
    std::uint8_t* GetHeadData() noexcept { return reinterpret_cast<std::uint8_t*>(&head_); }
    
    // Set header data.
    void SetHeadOpcode(std::uint16_t opcode) noexcept { head_.opcode_ = swap_endian(opcode); }
    void SetHeadSize(std::uint32_t size) noexcept { head_.size_ = swap_endian(size); }

    // Get body data.
    std::size_t GetBodySize() const noexcept { return body_.size(); }
    std::uint8_t* GetBodyData() noexcept { return body_.data(); }

    bool IsValidHeader() const noexcept;
    void ResizeBody(std::size_t size) { body_.resize(size); }
    std::vector<std::uint8_t> MakeBuffer();

  private:
    PacketHead head_{};
    std::vector<std::uint8_t> body_;
  };

  inline bool NetPacket::IsValidHeader() const noexcept
  { 
    return swap_endian(head_.opcode_) < packet_info::kMaxOpcode && 
           swap_endian(head_.size_) < packet_info::kMaxPayloadSize;
  }

  inline std::vector<std::uint8_t> NetPacket::MakeBuffer()
  {
    std::vector<std::uint8_t> res(body_.size() + sizeof(PacketHead));
    memcpy(res.data(), &head_, sizeof(PacketHead));
    memcpy(res.data() + sizeof(PacketHead), body_.data(), body_.size());
    
    return res;
  }

  template<PodType T>
  NetPacket& operator<<(NetPacket& packet, T value)
  {
    // Convert from host byte order to network byte order.
    value = swap_endian(value);

    // Allocate memmory and copy value into buffer
    std::size_t offset = packet.GetBodySize();
    packet.ResizeBody(offset + sizeof(T));
    std::memcpy(packet.GetBodyData() + offset, &value, sizeof(T));

    // Update size
    packet.SetHeadSize(packet.GetBodySize());

    return packet;
  }

  template<PodType T>
  NetPacket& operator>>(NetPacket& packet, T& value)
  {
    // Copy payload data into value and resize buffer.
    std::size_t offset = packet.GetBodySize() - sizeof(T);
    memcpy(&value, packet.GetBodyData() + offset, sizeof(T));
  
    // Convert from netwrok byte order to host byte order.
    value = swap_endian(value);

    // Update size
    packet.ResizeBody(offset);
    packet.SetHeadSize(offset);

    return packet;
  }

  NetPacket CreatePlayerPacket(std::size_t id, double x, double y, std::uint8_t width, std::uint8_t height);
  NetPacket AddPlayerPacket(std::size_t id, double x, double y, std::uint8_t width, std::uint8_t height);
  NetPacket RemovePlayerPacket(std::size_t id);
  NetPacket MovePlayerPacket(std::size_t id, double x, double y);
}
