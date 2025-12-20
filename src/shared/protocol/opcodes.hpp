#pragma once

#include <cstdint>
#include <cstring>

namespace ep
{
  enum class Opcodes : std::uint16_t {
    CreatePlayer            = 0x0003,
    SpawnPlayers            = 0x0004,
    MoveLeft                = 0x0012,
    MoveRight               = 0x0013,
    Jump                    = 0x0014,
    MovePlayer              = 0x0015,
    AddPlayer               = 0x0016,
    RemovePlayer            = 0x0017,
  };

  constexpr std::uint16_t to_uint16(Opcodes opcode)
  {
    return static_cast<std::uint16_t>(opcode);
  }

  constexpr Opcodes to_opcode(std::uint16_t opcode)
  {
    return static_cast<Opcodes>(opcode);
  }
}
