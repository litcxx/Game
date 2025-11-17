#pragma once

#include <cstdint>

namespace ep::protocol
{
  enum class Opcodes : uint16_t {
    MoveForward             = 0x0001,
    MoveLeft                = 0x0002,
    MoveBackward            = 0x0003,
    MoveRight               = 0x0004,
  };
}
