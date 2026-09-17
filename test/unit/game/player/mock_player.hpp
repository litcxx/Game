#pragma once

#include <gmock/gmock.h>

#include "player/i_player.hpp"

namespace ep::tests {
class MockPlayer : public ep::game::IPlayer {
  public:
    MOCK_METHOD(void, move, (double dx, double dy), (override));
    MOCK_METHOD(double, get_x, (), (const, noexcept, override));
    MOCK_METHOD(double, get_y, (), (const, noexcept, override));
    MOCK_METHOD(std::uint8_t, get_width, (), (const, noexcept, override));
    MOCK_METHOD(std::uint8_t, get_height, (), (const, noexcept, override));
    MOCK_METHOD(std::size_t, get_id, (), (const, noexcept, override));
};
}  // namespace ep::tests
