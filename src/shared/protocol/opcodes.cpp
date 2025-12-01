#include "opcodes.hpp"
#include "protocol/base_packet.hpp"

namespace ep
{
  NetPacket CreatePlayerPacket(std::size_t id, double x, double y)
  {
    NetPacket packet;
    packet.SetID(id);
    packet.SetPacketType(PacketType::Rpc);
    packet.SetHeadOpcode(to_uint16(Opcodes::CreatePlayer));
    packet << id << x << y;
    return std::move(packet);
  }

  NetPacket AddPlayerPacket(std::size_t id, double x, double y)
  {
    NetPacket packet;
    packet.SetID(id);
    packet.SetPacketType(PacketType::RpcOthers);
    packet.SetHeadOpcode(to_uint16(Opcodes::AddPlayer));
    packet << id << x << y;
    return std::move(packet);
  }

  NetPacket RmvPlayerPacket(std::size_t id)
  {
    NetPacket packet;
    packet.SetID(id);
    packet.SetPacketType(PacketType::RpcOthers);
    packet.SetHeadOpcode(to_uint16(Opcodes::RmvPlayer));
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
