#pragma once

#include <gmock/gmock.h>

#include "tile/i_box.hpp"

namespace lit::tests {
class MockBox : public lit::game::IBox {
  public:
    MOCK_METHOD(double, get_x, (), (const, noexcept, override));
    MOCK_METHOD(double, get_y, (), (const, noexcept, override));
    MOCK_METHOD(std::uint8_t, get_width, (), (const, noexcept, override));
    MOCK_METHOD(std::uint8_t, get_height, (), (const, noexcept, override));
};
}  // namespace lit::tests
