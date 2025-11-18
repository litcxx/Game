#pragma once

#include <gmock/gmock.h>

#include "player/i_player.hpp"

namespace ep::tests
{
  class MockPlayer : public ep::game::IPlayer {
  public:
    MOCK_METHOD(void, Move, (double dx, double dy, double dz), (override));
    MOCK_METHOD(double, GetX, (), (const, noexcept, override));
    MOCK_METHOD(double, GetY, (), (const, noexcept, override));
    MOCK_METHOD(double, GetZ, (), (const, noexcept, override));
    MOCK_METHOD(std::size_t, GetID, (), (const, noexcept, override));
  };
}
