#pragma once

#include "tile/i_box.hpp"

namespace ep::game
{
  class IPlayer : public IBox{
  public:
    virtual ~IPlayer() = default;

    virtual std::size_t GetID() const noexcept = 0;
    virtual void Move(double x, double y) = 0;
  };
}
