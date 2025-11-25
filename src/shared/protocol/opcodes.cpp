#include "opcodes.hpp"
#include "protocol/base_packet.hpp"

namespace ep
{
  net::NetPacket CreatePlayerPacket(std::size_t id, double x, double y)
  {
    net::NetPacket packet;
    packet.SetID(id);
    packet.SetPacketType(net::PacketType::Rpc);
    packet.SetHeadOpcode(to_uint16(ep::Opcodes::CreatePlayer));
    packet << id << x << y;
    return std::move(packet);
  }

  net::NetPacket AddPlayerPacket(std::size_t id, double x, double y)
  {
    net::NetPacket packet;
    packet.SetID(id);
    packet.SetPacketType(net::PacketType::RpcOthers);
    packet.SetHeadOpcode(to_uint16(ep::Opcodes::AddPlayer));
    packet << id << x << y;
    return std::move(packet);
  }

  net::NetPacket MovePlayerPacket(std::size_t id, double x, double y)
  {
    net::NetPacket packet;
    packet.SetID(id);
    packet.SetPacketType(net::PacketType::Broadcast);
    packet.SetHeadOpcode(to_uint16(ep::Opcodes::MovePlayer));
    packet << id << x << y;
    return std::move(packet);
  }
}
