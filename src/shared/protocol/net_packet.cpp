#include "net_packet.hpp"

#include "opcodes.hpp"

namespace ep
{
  NetPacket::NetPacket(Opcodes opcode) :
    head_{swap_endian(to_uint16(opcode)), 0}
  {}

  NetPacket::NetPacket(NetPacket&& other) :
    head_(other.head_),
    body_(std::move(other.body_)) 
  {
    other.head_.size_ = 0;
  }

  NetPacket& NetPacket::operator=(NetPacket&& other)
  {
    if (this == &other) return *this;
  
    head_ = other.head_;
    body_ = std::move(other.body_);
    other.head_.size_ = 0;

    return *this;
  }


  std::size_t NetPacket::GetBodySize() const noexcept
  {
    if (!body_)
      return 0;
    return body_->size();
  }

  std::uint8_t* NetPacket::GetBodyData() noexcept
  {
    if (!body_)
      return nullptr;
    return body_->data();
  }

  void NetPacket::ResizeBody(std::size_t size)
  {
    if (!body_)
      body_ = std::make_unique<std::vector<std::uint8_t>>(size);
    else
      body_->resize(size);
  }

  bool NetPacket::IsValidHeader() const noexcept
  { 
    return swap_endian(head_.opcode_) < packet_info::kMaxOpcode && 
           swap_endian(head_.size_) < packet_info::kMaxPayloadSize;
  }

  std::vector<std::uint8_t> NetPacket::MakeBuffer()
  {
    if (!body_)
      return {};

    std::vector<std::uint8_t> res(body_->size() + sizeof(PacketHead));
    memcpy(res.data(), &head_, sizeof(PacketHead));
    memcpy(res.data() + sizeof(PacketHead), body_->data(), body_->size());
    
    return res;
  }
  NetPacket CreatePlayerPacket(std::size_t id, double x, double y, std::uint8_t width, std::uint8_t height)
  {
    NetPacket packet;
    packet.SetHeadOpcode(to_uint16(Opcodes::CreatePlayer));
    packet << id << x << y << width << height; 
    return packet;
  }

  NetPacket AddPlayerPacket(std::size_t id, double x, double y, std::uint8_t width, std::uint8_t height)
  {
    NetPacket packet;
    packet.SetHeadOpcode(to_uint16(Opcodes::AddPlayer));
    packet << id << x << y << width << height;
    return packet;
  }

  NetPacket RemovePlayerPacket(std::size_t id)
  {
    NetPacket packet;
    packet.SetHeadOpcode(to_uint16(Opcodes::RemovePlayer));
    packet << id;
    return packet;
  }

  NetPacket MovePlayerPacket(std::size_t id, double x, double y)
  {
    NetPacket packet;
    packet.SetHeadOpcode(to_uint16(Opcodes::MovePlayer));
    packet << id << x << y;
    return packet;
  }
}
