#pragma once

#include <gmock/gmock.h>

#include "tile/i_box.hpp"

namespace ep::tests
{
  class MockBox: public ep::game::IBox {
  public:
    MOCK_METHOD(double, GetX, (), (const, noexcept, override));
    MOCK_METHOD(double, GetY, (), (const, noexcept, override));
    MOCK_METHOD(std::uint8_t , GetWidth, (), (const, noexcept, override));
    MOCK_METHOD(std::uint8_t , GetHeight, (), (const, noexcept, override));
  };
}
