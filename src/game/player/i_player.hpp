#pragma once

#include <cstdint>

namespace ep::game
{
  class IPlayer {
  public:
    virtual ~IPlayer() = default;

    virtual void Move(double dx, double dy, double dz) = 0;

    virtual double GetX() const noexcept = 0;
    virtual double GetY() const noexcept = 0;
    virtual double GetZ() const noexcept = 0;
    virtual std::size_t GetID() const noexcept = 0;
  };
}
