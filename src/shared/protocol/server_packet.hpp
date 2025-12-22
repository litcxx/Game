#pragma once

#include "net_packet.hpp"

namespace ep
{
  enum class PacketType : uint8_t {
    Broadcast,
    Rpc,
    RpcOthers,
  };

  class ServerPacket {
  public:
    ServerPacket(NetPacket packet, std::size_t id, PacketType type = PacketType::Broadcast) :
      packet_(std::move(packet)),
      id_(id),
      type_(type)
    {}
    ServerPacket(const ServerPacket&) = delete;
    ServerPacket& operator=(const ServerPacket&) = delete;
    ~ServerPacket() = default;

    NetPacket GetNetPacket() noexcept { return std::move(packet_); }
    std::size_t GetID() const noexcept { return id_; }
    PacketType GetType() const noexcept { return type_; }

  private:
    NetPacket packet_;
    std::size_t id_;
    PacketType type_;
  };
}
