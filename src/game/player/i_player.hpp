#pragma once

namespace ep::game
{
  class IPlayer {
  public:
    virtual ~IPlayer() = default;

    virtual void Move(float dx, float dy, float dz) = 0;
  };
}
