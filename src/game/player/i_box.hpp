#pragma once

#include <cstdint>

namespace ep::game
{
  class IBox {
  public:
    virtual ~IBox() = default;

    virtual double GetX() const noexcept = 0;
    virtual double GetY() const noexcept = 0;
    virtual std::uint8_t GetWidth() const noexcept = 0;
    virtual std::uint8_t GetHeight() const noexcept = 0;
  };
}
