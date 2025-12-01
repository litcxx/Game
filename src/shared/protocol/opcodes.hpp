#pragma once

#include <cstdint>
#include <cstring>
#include <memory>

#include "protocol/base_packet.hpp"

namespace ep
{
  enum class Opcodes : std::uint16_t {
    CreatePlayer            = 0x0003,
    SpawnPlayers            = 0x0004,
    MoveForward             = 0x0011,
    MoveLeft                = 0x0012,
    MoveBackward            = 0x0013,
    MoveRight               = 0x0014,
    MovePlayer              = 0x0015,
    AddPlayer               = 0x0016,
    RmvPlayer               = 0x0017,
  };

  constexpr std::uint16_t to_uint16(Opcodes opcode)
  {
    return static_cast<std::uint16_t>(opcode);
  }

  constexpr Opcodes to_opcode(std::uint16_t opcode)
  {
    return static_cast<Opcodes>(opcode);
  }

  NetPacket CreatePlayerPacket(std::size_t id, double x, double y);
  NetPacket AddPlayerPacket(std::size_t id, double x, double y);
  NetPacket RmvPlayerPacket(std::size_t id);
  NetPacket MovePlayerPacket(std::size_t id, double x, double y);
  std::shared_ptr<std::uint8_t[]> PacketBuffer(NetPacket packet);
}
