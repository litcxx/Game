#pragma once

#include <cstdint>

namespace ep
{
  enum class Opcodes : uint16_t {
    MoveForward             = 0x0011,
    MoveLeft                = 0x0012,
    MoveBackward            = 0x0013,
    MoveRight               = 0x0014,
    MovePlayer              = 0x0015,
  };

  constexpr uint16_t to_uint16(ep::Opcodes opcode)
  {
    return static_cast<uint16_t>(opcode);
  }
}
