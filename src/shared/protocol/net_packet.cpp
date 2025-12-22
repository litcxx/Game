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
