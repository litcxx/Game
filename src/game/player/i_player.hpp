#pragma once

#include "tile/i_box.hpp"

namespace ep::game
{
  class IPlayer : public IBox{
  public:
    virtual ~IPlayer() = default;

    virtual std::size_t GetID() const noexcept = 0;
    virtual void Move(double x, double y) = 0;
    virtual double GetVelX() const noexcept = 0;
    virtual double GetVelY() const noexcept = 0;
    virtual void SetVel(double vel_x, double vel_y) noexcept = 0;
  };
}
