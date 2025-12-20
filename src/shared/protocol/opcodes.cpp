#include "opcodes.hpp"

namespace ep
{
  NetPacket CreatePlayerPacket(std::size_t id, double x, double y, std::uint8_t width, std::uint8_t height)
  {
    NetPacket packet;
    packet.SetID(id);
    packet.SetPacketType(PacketType::Rpc);
    packet.SetHeadOpcode(to_uint16(Opcodes::CreatePlayer));
    packet << id << x << y << width << height; 
    return std::move(packet);
  }

  NetPacket AddPlayerPacket(std::size_t id, double x, double y, std::uint8_t width, std::uint8_t height)
  {
    NetPacket packet;
    packet.SetID(id);
    packet.SetPacketType(PacketType::RpcOthers);
    packet.SetHeadOpcode(to_uint16(Opcodes::AddPlayer));
    packet << id << x << y << width << height;
    return std::move(packet);
  }

  NetPacket RemovePlayerPacket(std::size_t id)
  {
    NetPacket packet;
    packet.SetID(id);
    packet.SetPacketType(PacketType::RpcOthers);
    packet.SetHeadOpcode(to_uint16(Opcodes::RemovePlayer));
    packet << id;
    return std::move(packet);
  }

  NetPacket MovePlayerPacket(std::size_t id, double x, double y)
  {
    NetPacket packet;
    packet.SetID(id);
    packet.SetPacketType(PacketType::Broadcast);
    packet.SetHeadOpcode(to_uint16(Opcodes::MovePlayer));
    packet << id << x << y;
    return std::move(packet);
  }
}
